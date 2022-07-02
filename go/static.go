package libfilter

// TODO: union, intersection

// #cgo CFLAGS: -march=native
// #cgo LDFLAGS: lib/libfilter.a -lm
// #include <filter/static.h>
import "C"
import "runtime"
import "unsafe"

type StaticFilter = C.libfilter_static

func NewStaticFilter(count uint64, hashes []uint64) *StaticFilter {
	b := new(StaticFilter)
	*b = C.libfilter_static_construct(C.uint64_t(count), (*C.uint64_t)(unsafe.Pointer(&hashes[0])))
	runtime.SetFinalizer(b, FreeStaticFilter)
	return b
}

func FreeStaticFilter(b *StaticFilter) {
	C.libfilter_static_destroy(*b)
}

func (b StaticFilter) FindHash(hash uint64) bool {
	return bool(C.libfilter_static_lookup(b, C.uint64_t(hash)))
}

func (b StaticFilter) Clone() *StaticFilter {
	result := new(StaticFilter)
	*result = C.libfilter_static_clone(b)
	runtime.SetFinalizer(result, FreeStaticFilter)
	return result
}
