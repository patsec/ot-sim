package util

func SliceContains[T comparable](s []T, v T) bool {
	for _, e := range s {
		if v == e {
			return true
		}
	}

	return false
}
