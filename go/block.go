package libfilter

// TODO: union, intersection

// #cgo CFLAGS: -march=native
// #cgo LDFLAGS: lib/libfilter.a -lm
// #include <filter/block.h>
import "C"
import "runtime"

type BlockFilter = C.libfilter_block

func BlockBytesNeeded(ndv float64, fpp float64) uint64 {
	return uint64(C.libfilter_block_bytes_needed(C.double(ndv), C.double(fpp)))
}

func NewBlockFilter(bytes uint64) *BlockFilter {
	b := new(BlockFilter)
	C.libfilter_block_init(C.uint64_t(bytes), b)
	runtime.SetFinalizer(b, FreeBlockFilter)
	return b
}

func FreeBlockFilter(b *BlockFilter) {
	C.libfilter_block_destruct(b)
}

func (b BlockFilter) AddHash(hash uint64) {
	C.libfilter_block_add_hash(C.uint64_t(hash), &b)
}

func (b BlockFilter) FindHash(hash uint64) bool {
	return bool(C.libfilter_block_find_hash(C.uint64_t(hash), &b))
}

func (b BlockFilter) Clone() *BlockFilter {
	result := new(BlockFilter)
	C.libfilter_block_clone(&b, result)
	runtime.SetFinalizer(result, FreeBlockFilter)
	return result
}
