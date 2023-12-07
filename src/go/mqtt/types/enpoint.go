package types

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"net/url"
	"os"
)

type Endpoint struct {
	Address string // broker
	URL     string // client

	CAPath   string
	KeyPath  string
	CertPath string

	URI   *url.URL
	Cert  tls.Certificate
	Roots *x509.CertPool

	Insecure bool
}

func (this *Endpoint) Validate() error {
	if this.Address == "" && this.URL == "" {
		return fmt.Errorf("must provide either endpoint URL or endpoint address")
	}

	if this.Address != "" && this.URL != "" {
		return fmt.Errorf("can only provide one of endpoint URL or endpoint address")
	}

	var err error

	if this.Address != "" { // broker
		if this.CertPath != "" || this.KeyPath != "" {
			if this.CertPath == "" || this.KeyPath == "" {
				return fmt.Errorf("must provide both 'certificate' and 'key' to enable TLS")
			}

			this.Cert, err = tls.LoadX509KeyPair(this.CertPath, this.KeyPath)
			if err != nil {
				return fmt.Errorf("loading MQTT module certificate and key: %w", err)
			}

			if this.CAPath != "" {
				caCert, err := os.ReadFile(this.CAPath)
				if err != nil {
					return fmt.Errorf("reading MQTT module CA certificate: %w", err)
				}

				this.Roots = x509.NewCertPool()

				if ok := this.Roots.AppendCertsFromPEM(caCert); !ok {
					return fmt.Errorf("failed to parse MQTT module CA certificate")
				}
			}
		} else {
			this.Insecure = true
		}
	}

	if this.URL != "" { // client
		this.URI, err = url.Parse(this.URL)
		if err != nil {
			return fmt.Errorf("parsing endpoint URL %s: %w", this.URL, err)
		}

		if this.URI.Scheme == "" {
			return fmt.Errorf("endpoint URL is missing a scheme (must be tcp, ssl, or tls)")
		}

		if this.URI.Scheme == "ssl" || this.URI.Scheme == "tls" {
			if this.CertPath == "" || this.KeyPath == "" {
				return fmt.Errorf("must provide 'certificate' and 'key' for MQTT module config when using ssl/tls")
			}

			this.Cert, err = tls.LoadX509KeyPair(this.CertPath, this.KeyPath)
			if err != nil {
				return fmt.Errorf("loading MQTT module certificate and key: %w", err)
			}

			if this.CAPath != "" {
				caCert, err := os.ReadFile(this.CAPath)
				if err != nil {
					return fmt.Errorf("reading MQTT module CA certificate: %w", err)
				}

				this.Roots = x509.NewCertPool()

				if ok := this.Roots.AppendCertsFromPEM(caCert); !ok {
					return fmt.Errorf("failed to parse MQTT module CA certificate")
				}
			}
		}
	}

	return nil
}
