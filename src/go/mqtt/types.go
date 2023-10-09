package mqtt

import (
	"bytes"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"net/url"
	"os"
	"text/template"
)

type endpoint struct {
	url string

	caPath   string
	keyPath  string
	certPath string

	uri   *url.URL
	cert  tls.Certificate
	roots *x509.CertPool

	insecure bool
}

func (this *endpoint) validate() error {
	var err error

	this.uri, err = url.Parse(this.url)
	if err != nil {
		return fmt.Errorf("parsing endpoint URL %s: %w", this.url, err)
	}

	if this.uri.Scheme == "" {
		return fmt.Errorf("endpoint URL is missing a scheme (must be tcp, ssl, or tls)")
	}

	if this.uri.Scheme == "ssl" || this.uri.Scheme == "tls" {
		if this.certPath == "" || this.keyPath == "" {
			return fmt.Errorf("must provide 'certificate' and 'key' for MQTT module config when using ssl/tls")
		}

		this.cert, err = tls.LoadX509KeyPair(this.certPath, this.keyPath)
		if err != nil {
			return fmt.Errorf("loading MQTT module certificate and key: %w", err)
		}

		if this.caPath != "" {
			caCert, err := os.ReadFile(this.caPath)
			if err != nil {
				return fmt.Errorf("reading MQTT module CA certificate: %w", err)
			}

			this.roots = x509.NewCertPool()

			if ok := this.roots.AppendCertsFromPEM(caCert); !ok {
				return fmt.Errorf("failed to parse MQTT module CA certificate")
			}
		}
	}

	return nil
}

// publication payload data
type data struct {
	Epoch     int64
	Timestamp string
	Client    string
	Topic     string
	Value     any
}

func (this data) execute(tmpl *template.Template) (string, error) {
	var buf bytes.Buffer

	if err := tmpl.Execute(&buf, this); err != nil {
		return "", fmt.Errorf("executing template: %w", err)
	}

	return buf.String(), nil
}
