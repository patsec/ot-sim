package cpu

import "context"

type (
	elasticEndpoint struct{}
	elasticIndex    struct{}
	lokiEndpoint    struct{}
)

func ctxSetElasticEndpoint(ctx context.Context, endpoint string) context.Context {
	return context.WithValue(ctx, elasticEndpoint{}, endpoint)
}

func ctxGetElasticEndpoint(ctx context.Context) string {
	value := ctx.Value(elasticEndpoint{})

	if value == nil {
		return ""
	}

	endpoint, ok := value.(string)
	if !ok {
		return ""
	}

	return endpoint
}

func ctxSetElasticIndex(ctx context.Context, index string) context.Context {
	return context.WithValue(ctx, elasticIndex{}, index)
}

func ctxGetElasticIndex(ctx context.Context) string {
	value := ctx.Value(elasticIndex{})

	if value == nil {
		return ""
	}

	index, ok := value.(string)
	if !ok {
		return ""
	}

	return index
}

func ctxSetLokiEndpoint(ctx context.Context, endpoint string) context.Context {
	return context.WithValue(ctx, lokiEndpoint{}, endpoint)
}

func ctxGetLokiEndpoint(ctx context.Context) string {
	value := ctx.Value(lokiEndpoint{})

	if value == nil {
		return ""
	}

	endpoint, ok := value.(string)
	if !ok {
		return ""
	}

	return endpoint
}
