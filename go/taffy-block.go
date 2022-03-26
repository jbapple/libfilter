package libfilter

// TODO: union, intersection

// #cgo CFLAGS: -march=native
// #cgo LDFLAGS: lib/libfilter.a -lm
// #include <filter/taffy-block.h>
import "C"
import "runtime"

type TaffyBlockFilter = C.libfilter_taffy_block

func NewTaffyBlockFilter(ndv uint64, fpp float64) *TaffyBlockFilter {
	b := new(TaffyBlockFilter)
	C.libfilter_taffy_block_init(C.uint64_t(ndv), C.double(fpp), b)
	runtime.SetFinalizer(b, FreeTaffyBlockFilter)
	return b
}

func FreeTaffyBlockFilter(b *TaffyBlockFilter) {
	C.libfilter_taffy_block_destruct(b)
}

func (b TaffyBlockFilter) AddHash(hash uint64) {
	C.libfilter_taffy_block_add_hash(&b, C.uint64_t(hash))
}

func (b TaffyBlockFilter) FindHash(hash uint64) bool {
	return bool(C.libfilter_taffy_block_find_hash(&b, C.uint64_t(hash)))
}

func (b TaffyBlockFilter) Clone() TaffyBlockFilter {
	return C.libfilter_taffy_block_clone(&b)
}
