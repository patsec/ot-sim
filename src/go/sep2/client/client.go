package client

import (
	"context"

	"github.com/beevik/etree"
)

type SEP2Client struct {
	name string
}

func New(name string) *SEP2Client {
	return &SEP2Client{
		name: name,
	}
}

func (this SEP2Client) Name() string {
	return this.name
}

func (this *SEP2Client) Configure(e *etree.Element) error {
	return nil
}

func (this *SEP2Client) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	return nil
}
