// Package mqtt implements an MQTTClient client as a module.
package mqtt

import (
	"bytes"
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"text/template"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	"github.com/cenkalti/backoff"
	MQTT "github.com/eclipse/paho.mqtt.golang"
)

/*
<mqtt>
	<!-- order of endpoint elements signifies priority -->
	<endpoint>
		<url>ssl://broker-1.example.com:8883</url>
		<tls>
			<ca>/etc/ot-sim/root.pem</ca>
			<key>/etc/ot-sim/broker-1-client.key</key>
			<certificate>/etc/ot-sim/broker-1-client.crt</certificate>
		</tls>
	</endpoint>
	<endpoint>
		<url>ssl://broker-2.example.com:8883</url>
		<tls>
			<ca>/etc/ot-sim/root.pem</ca>
			<key>/etc/ot-sim/broker-2-client.key</key>
			<certificate>/etc/ot-sim/broker-2-client.crt</certificate>
		</tls>
	</endpoint>
	<endpoint>tcp://broker.example.com:1883</endpoint> <!-- alternative way to specify -->
	<client-id>ot-sim-jitp-test</client-id>
	<period>5s</period>
	<tag>bus-692.voltage</tag>
</mqtt>
*/

func init() {
	otsim.AddModuleFactory("mqtt", new(Factory))
}

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "mqtt")
	return New(name), nil
}

type MQTTClient struct {
	sync.RWMutex

	pubEndpoint string

	endpoints []endpoint
	period    time.Duration

	name string
	id   string

	topics map[string]string
	values map[string]float64

	// index of endpoint currently in use
	endpoint int
	client   MQTT.Client

	payloadTmpl   *template.Template
	timestampTmpl string
}

func New(name string) *MQTTClient {
	return &MQTTClient{
		name:          name,
		topics:        make(map[string]string),
		values:        make(map[string]float64),
		payloadTmpl:   template.Must(template.New("payload").Parse(`{{ .Value }}`)),
		timestampTmpl: time.RFC3339,
	}
}

func (this MQTTClient) Name() string {
	return this.name
}

func (this *MQTTClient) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "endpoint":
			var endpoint endpoint

			if len(child.ChildElements()) == 0 {
				endpoint.url = child.Text()
			} else {
				for _, child := range child.ChildElements() {
					switch child.Tag {
					case "url":
						endpoint.url = child.Text()
					case "tls":
						insecure := child.SelectAttrValue("insecure", "false")
						endpoint.insecure, _ = strconv.ParseBool(insecure)

						for _, child := range child.ChildElements() {
							switch child.Tag {
							case "ca":
								endpoint.caPath = child.Text()
							case "key":
								endpoint.keyPath = child.Text()
							case "certificate":
								endpoint.certPath = child.Text()
							}
						}
					}
				}
			}

			this.endpoints = append(this.endpoints, endpoint)
		case "client-id":
			this.id = child.Text()
		case "period":
			var err error
			if this.period, err = time.ParseDuration(child.Text()); err != nil {
				return fmt.Errorf("invalid period duration '%s': %w", child.Text(), err)
			}
		case "tag":
			var (
				tag   = child.Text()
				topic = child.SelectAttrValue("topic", strings.ReplaceAll(tag, ".", "/"))
			)

			this.topics[tag] = topic
			this.values[tag] = 0.0
		case "payload-template":
			var err error

			this.payloadTmpl, err = template.New("payload").Parse(child.Text())
			if err != nil {
				return fmt.Errorf("parsing payload template: %w", err)
			}

			this.timestampTmpl = child.SelectAttrValue("timestamp", this.timestampTmpl)
		}
	}

	if this.id == "" {
		return fmt.Errorf("must provide 'client-id' for MQTT module config")
	}

	for idx, endpoint := range this.endpoints {
		this.log("[DEBUG] endpoint pre validation: %+v", endpoint)

		if err := endpoint.validate(); err != nil {
			return fmt.Errorf("validating endpoint: %w", err)
		}

		this.log("[DEBUG] endpoint post validation: %+v", endpoint)

		this.endpoints[idx] = endpoint
	}

	return nil
}

func (this *MQTTClient) Run(ctx context.Context, pubEndpoint, _ string) error {
	// Use ZeroMQ PUB endpoint specified in `mqtt` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	if len(this.endpoints) == 0 {
		return fmt.Errorf("no MQTT broker endpoints provided")
	}

	subscriber := msgbus.MustNewSubscriber(pubEndpoint)
	subscriber.AddStatusHandler(this.handleMsgBusStatus)
	subscriber.Start("RUNTIME")

	this.connectAndRun(ctx)

	return nil
}

func (this *MQTTClient) connectAndRun(ctx context.Context) {
	cctx, cancel := context.WithCancel(ctx)
	backoff := backoff.NewExponentialBackOff()

	for {
		if err := this.connect(ctx, cancel); err == nil {
			break
		}

		time.Sleep(backoff.NextBackOff())
	}

	go this.run(cctx)
}

func (this *MQTTClient) connect(ctx context.Context, cancel context.CancelFunc) error {
	// circle back around to beginning of endpoint list
	if this.endpoint == len(this.endpoints) {
		this.endpoint = 0
	}

	endpoint := this.endpoints[this.endpoint]
	this.endpoint++

	opts := MQTT.NewClientOptions()

	opts.AddBroker(endpoint.url).SetClientID(this.id).SetCleanSession(true)
	opts.SetKeepAlive(5 * time.Second).SetAutoReconnect(false).SetConnectRetry(false).SetConnectTimeout(10 * time.Second)

	opts.SetConnectionLostHandler(this.lostConnectionHandler(ctx, cancel))

	if endpoint.uri.Scheme == "ssl" || endpoint.uri.Scheme == "tls" {
		opts.SetTLSConfig(&tls.Config{
			ServerName:         endpoint.uri.Hostname(),
			RootCAs:            endpoint.roots,
			Certificates:       []tls.Certificate{endpoint.cert},
			InsecureSkipVerify: endpoint.insecure,
		})
	}

	this.client = MQTT.NewClient(opts)

	if token := this.client.Connect(); token.Wait() && token.Error() != nil {
		this.log("[ERROR] connecting to MQTT broker at %s: %v", endpoint.url, token.Error())
		return fmt.Errorf("connecting to MQTT broker: %w", token.Error())
	}

	this.log("[DEBUG] connected to MQTT broker at %s", endpoint.url)

	return nil
}

func (this *MQTTClient) run(ctx context.Context) {
	if this.period == 0 {
		return
	}

	ticker := time.NewTicker(this.period)

	for {
		select {
		case <-ctx.Done():
			ticker.Stop()

			this.log("[ERROR] stopping publish loop: %v", ctx.Err())

			return
		case <-ticker.C:
			this.RLock()

			for tag, value := range this.values {
				go this.publish(tag, value)
			}

			this.RUnlock()
		}
	}
}

func (this *MQTTClient) publish(tag string, value float64) {
	var (
		tstamp = time.Now().UTC()
		topic  = this.topics[tag]

		buf bytes.Buffer
	)

	pdata := data{
		Epoch:     tstamp.Unix(),
		Timestamp: tstamp.Format(this.timestampTmpl),
		Client:    this.id,
		Topic:     topic,
		Value:     value,
	}

	if err := this.payloadTmpl.Execute(&buf, pdata); err != nil {
		this.log("[ERROR] executing payload template: %v", err)
		return
	}

	token := this.client.Publish(topic, 0, false, buf.String())

	this.log("[DEBUG] publishing %s --> %s to MQTT broker", topic, buf.String())

	if token.Wait() && token.Error() != nil {
		this.log("[ERROR] publishing topic %s to MQTT broker: %v", topic, token.Error())
	} else {
		this.log("[DEBUG] published %s --> %f to MQTT broker", topic, value)
	}
}

func (this *MQTTClient) handleMsgBusStatus(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	status, err := env.Status()
	if err != nil {
		if !errors.Is(err, msgbus.ErrKindNotStatus) {
			this.log("[ERROR] getting Status message from envelope: %v", err)
		}

		return
	}

	this.Lock()
	defer this.Unlock()

	for _, point := range status.Measurements {
		if _, ok := this.values[point.Tag]; ok {
			this.values[point.Tag] = point.Value

			if this.period == 0 {
				go this.publish(point.Tag, point.Value)
			}
		}
	}
}

func (this *MQTTClient) lostConnectionHandler(ctx context.Context, cancel context.CancelFunc) MQTT.ConnectionLostHandler {
	return func(client MQTT.Client, err error) {
		this.log("[ERROR] connection to MQTT broker lost: %v", err)

		cancel()

		this.connectAndRun(ctx)
	}
}

func (this MQTTClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
