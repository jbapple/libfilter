package libfilter

// TODO: union

// #cgo LDFLAGS: lib/libfilter.a
// #include <filter/taffy-cuckoo.h>
import "C"
import "runtime"

type TaffyCuckooFilter = C.libfilter_taffy_cuckoo
type FrozenTaffyCuckooFilter = C.libfilter_frozen_taffy_cuckoo

func NewTaffyCuckooFilter(bytes uint64) *TaffyCuckooFilter {
	b := new(TaffyCuckooFilter)
	C.libfilter_taffy_cuckoo_init(C.uint64_t(bytes), b)
	runtime.SetFinalizer(b, FreeTaffyCuckooFilter)
	return b
}

func (b TaffyCuckooFilter) Freeze() *FrozenTaffyCuckooFilter {
	f := new(FrozenTaffyCuckooFilter)
	C.libfilter_taffy_cuckoo_freeze_init(&b, f)
	runtime.SetFinalizer(f, FreeFrozenTaffyCuckooFilter)
	return f
}

func FreeFrozenTaffyCuckooFilter(b *FrozenTaffyCuckooFilter) {
	C.libfilter_frozen_taffy_cuckoo_destruct(b)
}

func FreeTaffyCuckooFilter(b *TaffyCuckooFilter) {
	C.libfilter_taffy_cuckoo_destruct(b)
}

func (b TaffyCuckooFilter) AddHash(hash uint64) {
	C.libfilter_taffy_cuckoo_add_hash(&b, C.uint64_t(hash))
}

func (b TaffyCuckooFilter) FindHash(hash uint64) bool {
	return bool(C.libfilter_taffy_cuckoo_find_hash(&b, C.uint64_t(hash)))
}

func (b FrozenTaffyCuckooFilter) FindHash(hash uint64) bool {
	return bool(C.libfilter_frozen_taffy_cuckoo_find_hash(&b, C.uint64_t(hash)))
}

func (b TaffyCuckooFilter) Clone() *TaffyCuckooFilter {
	result := new(TaffyCuckooFilter)
	runtime.SetFinalizer(result, FreeTaffyCuckooFilter)
	// TODO: handle error
	C.libfilter_taffy_cuckoo_clone(&b, result)
	return result;
}
