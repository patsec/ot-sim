// Package mqtt implements an MQTTClient client as a module.
package mqtt

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"errors"
	"fmt"
	"io/ioutil"
	"net/url"
	"strings"
	"sync"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	MQTT "github.com/eclipse/paho.mqtt.golang"
)

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
	endpoint    string
	period      time.Duration

	name string
	id   string
	uri  *url.URL

	topics map[string]string
	values map[string]float64

	client MQTT.Client
	cert   tls.Certificate
	roots  *x509.CertPool
}

func New(name string) *MQTTClient {
	return &MQTTClient{
		name:   name,
		topics: make(map[string]string),
		values: make(map[string]float64),
	}
}

func (this MQTTClient) Name() string {
	return this.name
}

func (this *MQTTClient) Configure(e *etree.Element) error {
	var cert, key, ca string

	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "endpoint":
			this.endpoint = child.Text()
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
		case "certificate":
			cert = child.Text()
		case "key":
			key = child.Text()
		case "ca":
			ca = child.Text()
		}
	}

	if this.id == "" {
		return fmt.Errorf("must provide 'client-id' for MQTT module config")
	}

	var err error

	this.uri, err = url.Parse(this.endpoint)
	if err != nil {
		return fmt.Errorf("parsing endpoint URL %s: %w", this.endpoint, err)
	}

	if this.uri.Scheme == "ssl" || this.uri.Scheme == "tls" {
		if cert == "" || key == "" || ca == "" {
			return fmt.Errorf("must provide 'certificate', 'key', and 'ca' for MQTT module config when using ssl/tls")
		}

		this.cert, err = tls.LoadX509KeyPair(cert, key)
		if err != nil {
			return fmt.Errorf("loading MQTT module certificate and key: %w", err)
		}

		caCert, err := ioutil.ReadFile(ca)
		if err != nil {
			return fmt.Errorf("reading MQTT module CA certificate: %w", err)
		}

		this.roots = x509.NewCertPool()

		if ok := this.roots.AppendCertsFromPEM(caCert); !ok {
			return fmt.Errorf("failed to parse MQTT module CA certificate")
		}
	}

	if this.period == 0 {
		this.period = 5 * time.Second
	}

	return nil
}

func (this *MQTTClient) Run(ctx context.Context, pubEndpoint, _ string) error {
	// Use ZeroMQ PUB endpoint specified in `mqtt` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	subscriber := msgbus.MustNewSubscriber(pubEndpoint)
	subscriber.AddStatusHandler(this.handleMsgBusStatus)
	subscriber.Start("RUNTIME")

	opts := MQTT.NewClientOptions()
	opts.AddBroker(this.endpoint).SetClientID(this.id).SetCleanSession(true)

	if this.uri.Scheme == "ssl" || this.uri.Scheme == "tls" {
		opts.SetTLSConfig(&tls.Config{ServerName: this.uri.Hostname(), RootCAs: this.roots, Certificates: []tls.Certificate{this.cert}})
	}

	this.client = MQTT.NewClient(opts)

	if token := this.client.Connect(); token.Wait() && token.Error() != nil {
		this.log("[ERROR] connecting to MQTT broker at %s: %v", this.endpoint, token.Error())
		return fmt.Errorf("connectin to MQTT broker: %w", token.Error())
	}

	go func() {
		ticker := time.NewTicker(this.period)

		for {
			select {
			case <-ctx.Done():
				subscriber.Stop()
				ticker.Stop()

				return
			case <-ticker.C:
				for tag, val := range this.values {
					topic := this.topics[tag]
					token := this.client.Publish(topic, 0, false, fmt.Sprint(val))

					this.log("[DEBUG] publishing %s --> %f to MQTT broker", topic, val)

					// TODO: should this be run in a separate goroutine?
					if token.Wait() && token.Error() != nil {
						this.log("[ERROR] publishing topic %s to MQTT broker: %v", topic, token.Error())
					} else {
						this.log("[DEBUG] published %s --> %f to MQTT broker", topic, val)
					}
				}
			}
		}
	}()

	return nil
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
		}
	}
}

func (this MQTTClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
