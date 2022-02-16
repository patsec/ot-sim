package util

import "context"

type configFileKey struct{}

func SetConfigFile(ctx context.Context, path string) context.Context {
	return context.WithValue(ctx, configFileKey{}, path)
}

func ConfigFile(ctx context.Context) (string, bool) {
	path, ok := ctx.Value(configFileKey{}).(string)
	return path, ok
}

func MustConfigFile(ctx context.Context) string {
	if path, ok := ConfigFile(ctx); ok {
		return path
	}

	panic("config file not set in context")
}
