/*
 * Copyright (c) 2013, 2015, Red Hat, Inc. and/or its affiliates.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#include "gc_implementation/shenandoah/shenandoahHeap.inline.hpp"
#include "gc_implementation/shenandoah/shenandoahBarrierSet.inline.hpp"
#include "memory/universe.hpp"
#include "utilities/array.hpp"

class UpdateRefsForOopClosure: public ExtendedOopClosure {

private:
  ShenandoahHeap* _heap;
public:
  UpdateRefsForOopClosure() {
    _heap = ShenandoahHeap::heap();
  }

  void do_oop(oop* p)       {
    _heap->maybe_update_oop_ref(p);
  }

  void do_oop(narrowOop* p) {
    Unimplemented();
  }

};

ShenandoahBarrierSet::ShenandoahBarrierSet() :
  BarrierSet()
{
  _kind = BarrierSet::ShenandoahBarrierSet;
}

void ShenandoahBarrierSet::print_on(outputStream* st) const {
  st->print("ShenandoahBarrierSet");
}

bool ShenandoahBarrierSet::is_a(BarrierSet::Name bsn) {
  return bsn == BarrierSet::ShenandoahBarrierSet;
}

bool ShenandoahBarrierSet::has_read_prim_array_opt() {
  return true;
}

bool ShenandoahBarrierSet::has_read_prim_barrier() {
  return false;
}

bool ShenandoahBarrierSet::has_read_ref_array_opt() {
  return true;
}

bool ShenandoahBarrierSet::has_read_ref_barrier() {
  return false;
}

bool ShenandoahBarrierSet::has_read_region_opt() {
  return true;
}

bool ShenandoahBarrierSet::has_write_prim_array_opt() {
  return true;
}

bool ShenandoahBarrierSet::has_write_prim_barrier() {
  return false;
}

bool ShenandoahBarrierSet::has_write_ref_array_opt() {
  return true;
}

bool ShenandoahBarrierSet::has_write_ref_barrier() {
  return true;
}

bool ShenandoahBarrierSet::has_write_ref_pre_barrier() {
  return true;
}

bool ShenandoahBarrierSet::has_write_region_opt() {
  return true;
}

bool ShenandoahBarrierSet::is_aligned(HeapWord* hw) {
  return true;
}

void ShenandoahBarrierSet::read_prim_array(MemRegion mr) {
  Unimplemented();
}

void ShenandoahBarrierSet::read_prim_field(HeapWord* hw, size_t s){
  Unimplemented();
}

bool ShenandoahBarrierSet::read_prim_needs_barrier(HeapWord* hw, size_t s) {
  return false;
}

void ShenandoahBarrierSet::read_ref_array(MemRegion mr) {
  Unimplemented();
}

void ShenandoahBarrierSet::read_ref_field(void* v) {
  //    tty->print_cr("read_ref_field: v = "PTR_FORMAT, v);
  // return *v;
}

bool ShenandoahBarrierSet::read_ref_needs_barrier(void* v) {
  Unimplemented();
  return false;
}

void ShenandoahBarrierSet::read_region(MemRegion mr) {
  Unimplemented();
}

void ShenandoahBarrierSet::resize_covered_region(MemRegion mr) {
  Unimplemented();
}

void ShenandoahBarrierSet::write_prim_array(MemRegion mr) {
  Unimplemented();
}

void ShenandoahBarrierSet::write_prim_field(HeapWord* hw, size_t s , juint x, juint y) {
  Unimplemented();
}

bool ShenandoahBarrierSet::write_prim_needs_barrier(HeapWord* hw, size_t s, juint x, juint y) {
  Unimplemented();
  return false;
}

bool ShenandoahBarrierSet::need_update_refs_barrier() {
  ShenandoahHeap* heap = ShenandoahHeap::heap();
  return heap->concurrent_mark_in_progress() && heap->need_update_refs();
}

void ShenandoahBarrierSet::write_ref_array_work(MemRegion mr) {
  if (! need_update_refs_barrier()) return;
  ShenandoahHeap* heap = ShenandoahHeap::heap();
  for (HeapWord* word = mr.start(); word < mr.end(); word++) {
    oop* oop_ptr = (oop*) word;
    heap->maybe_update_oop_ref(oop_ptr);
  }
}

template <class T>
void ShenandoahBarrierSet::write_ref_array_pre_work(T* dst, int count) {

#ifdef ASSERT
    ShenandoahHeap *sh = (ShenandoahHeap*) Universe::heap();
    if (sh->is_in(dst) &&
        sh->heap_region_containing((HeapWord*) dst)->is_in_collection_set() &&
        ! sh->cancelled_concgc()) {
      tty->print_cr("dst = "PTR_FORMAT, p2i(dst));
      sh->heap_region_containing((HeapWord*) dst)->print();
      assert(false, "We should have fixed this earlier");
    }
#endif

  if (! JavaThread::satb_mark_queue_set().is_active()) return;
  // tty->print_cr("write_ref_array_pre_work: "PTR_FORMAT", "INT32_FORMAT, dst, count);
  T* elem_ptr = dst;
  for (int i = 0; i < count; i++, elem_ptr++) {
    T heap_oop = oopDesc::load_heap_oop(elem_ptr);
    if (!oopDesc::is_null(heap_oop)) {
      G1SATBCardTableModRefBS::enqueue(oopDesc::decode_heap_oop_not_null(heap_oop));
    }
    // tty->print_cr("write_ref_array_pre_work: oop: "PTR_FORMAT, heap_oop);
  }
}

void ShenandoahBarrierSet::write_ref_array_pre(oop* dst, int count, bool dest_uninitialized) {
  if (! dest_uninitialized) {
    write_ref_array_pre_work(dst, count);
  }
}

void ShenandoahBarrierSet::write_ref_array_pre(narrowOop* dst, int count, bool dest_uninitialized) {
  if (! dest_uninitialized) {
    write_ref_array_pre_work(dst, count);
  }
}

template <class T>
void ShenandoahBarrierSet::write_ref_field_pre_static(T* field, oop newVal) {
  T heap_oop = oopDesc::load_heap_oop(field);

#ifdef ASSERT
    ShenandoahHeap *sh = (ShenandoahHeap*) Universe::heap();
    if (sh->is_in(field) &&
        sh->heap_region_containing((HeapWord*)field)->is_in_collection_set() &&
        ! sh->cancelled_concgc()) {
      tty->print_cr("field = "PTR_FORMAT, p2i(field));
      sh->heap_region_containing((HeapWord*)field)->print();
      assert(false, "We should have fixed this earlier");
    }
#endif

  if (!oopDesc::is_null(heap_oop)) {
    G1SATBCardTableModRefBS::enqueue(oopDesc::decode_heap_oop(heap_oop));
    // tty->print_cr("write_ref_field_pre_static: v = "PTR_FORMAT" o = "PTR_FORMAT" old: "PTR_FORMAT, field, newVal, heap_oop);
  }
}

template <class T>
inline void ShenandoahBarrierSet::inline_write_ref_field_pre(T* field, oop newVal) {
  write_ref_field_pre_static(field, newVal);
}

// These are the more general virtual versions.
void ShenandoahBarrierSet::write_ref_field_pre_work(oop* field, oop new_val) {
  write_ref_field_pre_static(field, new_val);
}

void ShenandoahBarrierSet::write_ref_field_pre_work(narrowOop* field, oop new_val) {
  write_ref_field_pre_static(field, new_val);
}

void ShenandoahBarrierSet::write_ref_field_pre_work(void* field, oop new_val) {
  guarantee(false, "Not needed");
}

void ShenandoahBarrierSet::write_ref_field_work(void* v, oop o, bool release) {
  if (! need_update_refs_barrier()) return;
  assert (! UseCompressedOops, "compressed oops not supported yet");
  ShenandoahHeap::heap()->maybe_update_oop_ref((oop*) v);
  // tty->print_cr("write_ref_field_work: v = "PTR_FORMAT" o = "PTR_FORMAT, v, o);
}

void ShenandoahBarrierSet::write_region_work(MemRegion mr) {

  if (! need_update_refs_barrier()) return;

  // This is called for cloning an object (see jvm.cpp) after the clone
  // has been made. We are not interested in any 'previous value' because
  // it would be NULL in any case. But we *are* interested in any oop*
  // that potentially need to be updated.

  // tty->print_cr("write_region_work: "PTR_FORMAT", "PTR_FORMAT, mr.start(), mr.end());
  oop obj = oop(mr.start());
  assert(obj->is_oop(), "must be an oop");
  UpdateRefsForOopClosure cl;
  obj->oop_iterate(&cl);
}

oop ShenandoahBarrierSet::read_barrier(oop src) {
  return ShenandoahBarrierSet::resolve_oop_static(src);
}

oop ShenandoahBarrierSet::resolve_and_maybe_copy_oop_work(oop src) {
  ShenandoahHeap *sh = (ShenandoahHeap*) Universe::heap();
  assert(src != NULL, "only evacuated non NULL oops");

  if (sh->in_cset_fast_test((HeapWord*) src)) {
    return resolve_and_maybe_copy_oop_work2(src);
  } else {
    return src;
  }
}

oop ShenandoahBarrierSet::resolve_and_maybe_copy_oop_work2(oop src) {
  ShenandoahHeap *sh = (ShenandoahHeap*) Universe::heap();
  if (! sh->is_evacuation_in_progress()) {
    // We may get here through a barrier that just took a safepoint that
    // turned off evacuation. In this case, return right away.
    return ShenandoahBarrierSet::resolve_oop_static(src);
  }
  assert(src != NULL, "only evacuated non NULL oops");
  assert(sh->heap_region_containing(src)->is_in_collection_set(), "only evacuate objects in collection set");
  assert(! sh->heap_region_containing(src)->is_humongous(), "never evacuate humongous objects");
  // TODO: Consider passing thread from caller.
  oop dst = sh->evacuate_object(src, Thread::current());
#ifdef ASSERT
    if (ShenandoahTraceEvacuations) {
      tty->print_cr("src = "PTR_FORMAT" dst = "PTR_FORMAT" src = "PTR_FORMAT" src-2 = "PTR_FORMAT,
                 p2i((HeapWord*) src), p2i((HeapWord*) dst), p2i((HeapWord*) src), p2i(((HeapWord*) src) - 2));
    }
#endif
  assert(sh->is_in(dst), "result should be in the heap");
  return dst;
}

oop ShenandoahBarrierSet::resolve_and_maybe_copy_oopHelper(oop src) {
    if (src != NULL) {
      ShenandoahHeap *sh = (ShenandoahHeap*) Universe::heap();
      oop tmp = resolve_oop_static(src);
      if (! sh->is_evacuation_in_progress()) {
        return tmp;
      }
      return resolve_and_maybe_copy_oop_work(src);
    } else {
      return NULL;
    }
}

JRT_LEAF(oopDesc*, ShenandoahBarrierSet::write_barrier_c2(oopDesc* src))
  oop result = ((ShenandoahBarrierSet*) oopDesc::bs())->resolve_and_maybe_copy_oop_work2(oop(src));
  // tty->print_cr("called C2 write barrier with: %p result: %p copy: %d", (oopDesc*) src, (oopDesc*) result, src != result);
  return (oopDesc*) result;
JRT_END

IRT_LEAF(oopDesc*, ShenandoahBarrierSet::write_barrier_interp(oopDesc* src))
  oop result = ((ShenandoahBarrierSet*)oopDesc::bs())->resolve_and_maybe_copy_oop_work2(oop(src));
  // tty->print_cr("called interpreter write barrier with: %p result: %p", src, result);
  return (oopDesc*) result;
IRT_END

JRT_LEAF(oopDesc*, ShenandoahBarrierSet::write_barrier_c1(JavaThread* thread, oopDesc* src))
  oop result = ((ShenandoahBarrierSet*)oopDesc::bs())->resolve_and_maybe_copy_oop_work2(oop(src));
  // tty->print_cr("called static write barrier (2) with: "PTR_FORMAT" result: "PTR_FORMAT, p2i(src), p2i((oopDesc*)(result)));
  return (oopDesc*) result;
JRT_END

oop ShenandoahBarrierSet::write_barrier(oop src) {
    ShenandoahHeap *sh = (ShenandoahHeap*) Universe::heap();
    oop result;
    if (src != NULL && sh->is_in(src)) {
      result = resolve_and_maybe_copy_oopHelper(src);
      assert(sh->is_in(result), "result should be in the heap");
    } else {
      result = src;
    }
    assert(result == NULL || (sh->is_in(result) && result->is_oop()), "resolved oop must be NULL, or a valid oop in the heap");
    return result;
  }

oop ShenandoahBarrierSet::resolve_and_update_oop(oop* p, oop obj) {
  return resolve_and_update_oop_static(p, obj);
}

oop ShenandoahBarrierSet::resolve_and_update_oop(narrowOop* p, oop obj) {
  Unimplemented();
  return NULL;
}
