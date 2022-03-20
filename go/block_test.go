package block

import (
	"math/rand"
	"testing"
)

func TestEmpty(t *testing.T) {
	b := NewBlockFilter(123456)
	for i := 0; i < 1234567; i++ {
		h := rand.Uint64()
		if b.FindHash(h) {
		 	t.Error("Found", h)
		}
	}
}

func BenchmarkFind(context *testing.B) {
	b := NewBlockFilter(1234567)
	keys := make([]uint64, context.N)
	for i := 0; i < context.N; i++ {
		keys[i] = rand.Uint64()
	}
	context.ResetTimer()
	for i := 0; i < context.N; i++ {
		if b.FindHash(keys[i]) {
			context.FailNow()
		}
	}
}

func BenchmarkAdd(context *testing.B) {
	b := NewBlockFilter(1234567)
	keys := make([]uint64, context.N)
	for i := 0; i < context.N; i++ {
		keys[i] = rand.Uint64()
	}
	context.ResetTimer()
	for i := 0; i < context.N; i++ {
		b.AddHash(keys[i])
	}
}
