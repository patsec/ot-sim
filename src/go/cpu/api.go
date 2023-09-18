package cpu

import (
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"sync"
	"time"

	"github.com/patsec/ot-sim/msgbus"

	"github.com/gorilla/mux"
	"github.com/gorilla/websocket"
)

const (
	API_WS_WRITE_WAIT  = 10 * time.Second
	API_WS_PONG_WAIT   = 60 * time.Second
	API_WS_PING_PERIOD = (API_WS_PONG_WAIT * 9) / 10
	API_WS_PUB_PERIOD  = 5 * time.Second
)

type APIServer struct {
	sync.RWMutex

	points map[string]msgbus.Point

	subscriber *msgbus.Subscriber
	pusher     *msgbus.Pusher

	upgrader websocket.Upgrader
}

func NewAPIServer(pull, pub string) *APIServer {
	server := &APIServer{
		points:     make(map[string]msgbus.Point),
		subscriber: msgbus.MustNewSubscriber(pub),
		pusher:     msgbus.MustNewPusher(pull),
		upgrader: websocket.Upgrader{
			ReadBufferSize:  4096,
			WriteBufferSize: 4096,
		},
	}

	return server
}

func (this *APIServer) Start(endpoint, cert, key, ca string) error {
	log.Printf("[CPU] starting API server at %s/api/v1\n", endpoint)

	router := mux.NewRouter().StrictSlash(true)
	api := router.PathPrefix("/api/v1").Subrouter()

	api.HandleFunc("/query", this.handleQuery).Methods("GET")
	api.HandleFunc("/query/ws", this.handleQueryWS).Methods("GET") // order matters here
	api.HandleFunc("/query/{tag}", this.handleQuery).Methods("GET")
	api.HandleFunc("/write", this.handleWrite).Methods("POST")
	api.HandleFunc("/write/{tag}/{value}", this.handleWrite).Methods("POST")
	api.HandleFunc("/modules", this.handleModules).Methods("GET")
	api.HandleFunc("/modules/{name}", this.handleEnableModule).Methods("POST")
	api.HandleFunc("/modules/{name}", this.handleDisableModule).Methods("DELETE")

	server := http.Server{Addr: endpoint, Handler: router}

	if cert != "" && key != "" {
		if ca != "" {
			tlsConfig, err := buildTLSConfig(ca)
			if err != nil {
				return fmt.Errorf("building config for mutual TLS: %w", err)
			}

			server.TLSConfig = tlsConfig
		}

		go server.ListenAndServeTLS(cert, key)
	} else {
		go server.ListenAndServe()
	}

	this.subscriber.AddStatusHandler(this.HandleMsgBusStatus)
	this.subscriber.Start("RUNTIME")

	return nil
}

func (this *APIServer) HandleMsgBusStatus(env msgbus.Envelope) {
	status, err := env.Status()
	if err != nil {
		if errors.Is(err, msgbus.ErrKindNotStatus) {
			return
		}

		log.Printf("[CPU] [ERROR] getting Status message from envelope: %v\n", err)
	}

	this.Lock()
	defer this.Unlock()

	for _, point := range status.Measurements {
		this.points[point.Tag] = point
	}
}

// GET /query
// GET /query/{tag}
func (this *APIServer) handleQuery(w http.ResponseWriter, r *http.Request) {
	var (
		vars = mux.Vars(r)
		tag  = vars["tag"]
	)

	this.RLock()
	defer this.RUnlock()

	if tag == "" {
		var list []msgbus.Point

		for _, point := range this.points {
			list = append(list, point)
		}

		body, err := json.Marshal(map[string]interface{}{"points": list})
		if err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			return
		}

		w.Write(body)
		return
	}

	if point, ok := this.points[tag]; ok {
		body, err := json.Marshal(point)
		if err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			return
		}

		w.Write(body)
		return
	}

	w.WriteHeader(http.StatusBadRequest)
}

// GET /query/ws
func (this *APIServer) handleQueryWS(w http.ResponseWriter, r *http.Request) {
	this.upgrader.CheckOrigin = func(*http.Request) bool { return true }

	ws, err := this.upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("[CPU] [ERROR] [API] %v\n", err)
		return
	}

	go this.wsWriter(ws)
	this.wsReader(ws)
}

// POST /write
// POST /write/{tag}/{value}
func (this *APIServer) handleWrite(w http.ResponseWriter, r *http.Request) {
	var (
		vars  = mux.Vars(r)
		tag   = vars["tag"]
		value = vars["value"]
	)

	this.Lock()
	defer this.Unlock()

	var (
		status msgbus.Status
		update msgbus.Update
	)

	if tag != "" && value != "" {
		point := msgbus.Point{Tag: tag}

		if val, err := strconv.ParseFloat(value, 64); err == nil {
			point.Value = val
		} else {
			w.WriteHeader(http.StatusBadRequest)
			return
		}

		status = msgbus.Status{Measurements: []msgbus.Point{point}}
		update = msgbus.Update{Updates: []msgbus.Point{point}}
	} else {
		body, err := io.ReadAll(r.Body)
		if err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			return
		}

		if err := json.Unmarshal(body, &update); err != nil {
			w.WriteHeader(http.StatusBadRequest)
			return
		}

		status = msgbus.Status{Measurements: update.Updates}
	}

	env, err := msgbus.NewEnvelope("cpu-api", status)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return
	}

	if err := this.pusher.Push("RUNTIME", env); err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return
	}

	env, err = msgbus.NewEnvelope("cpu-api", update)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return
	}

	if err := this.pusher.Push("RUNTIME", env); err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusNoContent)
}

// GET /modules
func (this *APIServer) handleModules(w http.ResponseWriter, r *http.Request) {
	results := make(map[string]string)

	for _, mod := range modules {
		if mod.canceler == nil {
			results[mod.name] = "disabled"
		} else {
			results[mod.name] = "enabled"
		}
	}

	body, err := json.Marshal(results)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return
	}

	w.Write(body)
}

// POST /modules/{name}
func (this *APIServer) handleEnableModule(w http.ResponseWriter, r *http.Request) {
	var (
		vars = mux.Vars(r)
		name = vars["name"]
	)

	if mod, ok := modules[name]; ok {
		if mod.canceler != nil {
			http.Error(w, "module already enabled", http.StatusBadRequest)
		} else {
			if err := StartModule(mod.ctx, mod.name, mod.path, mod.args...); err != nil {
				log.Printf("[CPU] [ERROR] failed to enable module %s: %v\n", name, err)

				http.Error(w, "failed to enable module", http.StatusInternalServerError)
				return
			}

			w.WriteHeader(http.StatusNoContent)
		}
	} else {
		http.Error(w, "module not found", http.StatusBadRequest)
	}
}

// DELETE /modules/{name}
func (this *APIServer) handleDisableModule(w http.ResponseWriter, r *http.Request) {
	var (
		vars = mux.Vars(r)
		name = vars["name"]
	)

	if mod, ok := modules[name]; ok {
		if mod.canceler == nil {
			http.Error(w, "module already disabled", http.StatusBadRequest)
		} else {
			close(mod.canceler)

			w.WriteHeader(http.StatusNoContent)
		}
	} else {
		http.Error(w, "module not found", http.StatusBadRequest)
	}
}

func (this *APIServer) wsReader(ws *websocket.Conn) {
	defer ws.Close()

	ws.SetReadLimit(512)
	ws.SetReadDeadline(time.Now().Add(API_WS_PONG_WAIT))
	ws.SetPongHandler(func(string) error { ws.SetReadDeadline(time.Now().Add(API_WS_PONG_WAIT)); return nil })

	for {
		_, _, err := ws.ReadMessage()
		if err != nil {
			break
		}
	}
}

func (this *APIServer) wsWriter(ws *websocket.Conn) {
	var (
		pingTicker = time.NewTicker(API_WS_PING_PERIOD)
		pubTicker  = time.NewTicker(API_WS_PUB_PERIOD)
	)

	defer func() {
		pingTicker.Stop()
		pubTicker.Stop()
		ws.Close()
	}()

	for {
		select {
		case <-pingTicker.C:
			ws.SetWriteDeadline(time.Now().Add(API_WS_WRITE_WAIT))

			if err := ws.WriteMessage(websocket.PingMessage, []byte{}); err != nil {
				return
			}
		case <-pubTicker.C:
			var list []msgbus.Point

			this.RLock()

			for _, point := range this.points {
				list = append(list, point)
			}

			this.RUnlock()

			body, err := json.Marshal(map[string]interface{}{"points": list})
			if err != nil {
				return
			}

			if err := ws.WriteMessage(websocket.TextMessage, body); err != nil {
				return
			}
		}
	}
}

func buildTLSConfig(cert string) (*tls.Config, error) {
	ca, err := os.ReadFile(cert)
	if err != nil {
		return nil, fmt.Errorf("reading CA certificate file: %w", err)
	}

	pool := x509.NewCertPool()
	pool.AppendCertsFromPEM(ca)

	config := &tls.Config{
		ClientCAs:  pool,
		ClientAuth: tls.RequireAndVerifyClientCert,
	}

	return config, nil
}
