package libfilter

import (
	"math/rand"
	"testing"
)

func TestEmptyTaffyBlock(t *testing.T) {
	EmptyHelper(NewTaffyBlockFilter(123456, 0.01), t)
}

func TestEmptyTaffyCuckoo(t *testing.T) {
	EmptyHelper(NewTaffyCuckooFilter(123456), t)
}

func TestEmpty(t *testing.T) {
	EmptyHelper(NewBlockFilter(123456), t)
}

func EmptyHelper[U interface{ FindHash(uint64) bool }](b *U, t *testing.T) {
	for i := 0; i < 123456; i++ {
		h := rand.Uint64()
		if (*b).FindHash(h) {
			t.Log("Found", h)
			t.FailNow()
		}
	}
}

func TestCumulativeTaffyBlock(t *testing.T) {
	CumulativeHelper(NewTaffyBlockFilter(123456, 0.01), t)
}

func TestCumulativeTaffyCuckoo(t *testing.T) {
	CumulativeHelper(NewTaffyCuckooFilter(123456), t)
}

func TestCumulative(t *testing.T) {
	CumulativeHelper(NewBlockFilter(123456), t)
}

func CumulativeHelper[U interface {
	AddHash(uint64)
	FindHash(uint64) bool
}](b *U, t *testing.T) {
	const count = 1234
	keys := make([]uint64, count)
	for i := 0; i < count; i++ {
		keys[i] = rand.Uint64()
		(*b).AddHash(keys[i])
		for j := 0; j < i; j++ {
			if !(*b).FindHash(keys[j]) {
				t.Log("Not found", keys[j])
				t.FailNow()
			}
		}
	}
}

// func BenchmarkFind(context *testing.B) {
// 	b := NewBlockFilter(1234567)
// 	keys := make([]uint64, context.N)
// 	for i := 0; i < context.N; i++ {
// 		keys[i] = rand.Uint64()
// 	}
// 	context.ResetTimer()
// 	for i := 0; i < context.N; i++ {
// 		if b.FindHash(keys[i]) {
// 			context.FailNow()
// 		}
// 	}
// }

// func BenchmarkAdd(context *testing.B) {
// 	b := NewBlockFilter(1234567)
// 	keys := make([]uint64, context.N)
// 	for i := 0; i < context.N; i++ {
// 		keys[i] = rand.Uint64()
// 	}
// 	context.ResetTimer()
// 	for i := 0; i < context.N; i++ {
// 		b.AddHash(keys[i])
// 	}
// }

// func BenchmarkTaffyBlockAdd(context *testing.B) {
// 	b := NewTaffyBlockFilter(1, 0.01)
// 	keys := make([]uint64, context.N)
// 	for i := 0; i < context.N; i++ {
// 		keys[i] = rand.Uint64()
// 	}
// 	context.ResetTimer()
// 	for i := 0; i < context.N; i++ {
// 		b.AddHash(keys[i])
// 	}
// }

// func BenchmarkTaffyCuckooAdd(context *testing.B) {
// 	b := NewTaffyCuckooFilter(1)
// 	keys := make([]uint64, context.N)
// 	for i := 0; i < context.N; i++ {
// 		keys[i] = rand.Uint64()
// 	}
// 	context.ResetTimer()
// 	for i := 0; i < context.N; i++ {
// 		b.AddHash(keys[i])
// 	}
// }
