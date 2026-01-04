#ifndef MAP_RECORD_H
#define MAP_RECORD_H

template <typename K, typename V> struct MapRecord {
  K key;
  V value;
  MapRecord(K k, V v) : key(k), value(v) {}
};

#define OK_REC(t, t1, t2, v1, v2) (Result<t<t1, t2> >(new t<t1, t2>(v1, v2), ""))

#define ERR_REC(t, t1, t2, e) (Result<t<t1, t2> >(NULL, e))

#define TRY_REC(t, t1, t2, vt, vt1, vt2, v, r)                                 \
  if (!(r).error().empty()) {                                                  \
    return (Result<t<t1, t2> >(NULL, (r).error()));                            \
  } else {                                                                     \
    v = const_cast<vt<vt1, vt2> *>((r).value());                               \
  }

#define TRYF_REC(t, t1, t2, vt, vt1, vt2, v, r, f)                             \
  if (!(r).error().empty()) {                                                  \
    f return (Result<t<t1, t2> >(NULL, (r).error()));                          \
  } else {                                                                     \
    v = const_cast<vt<vt1, vt2> *>((r).value());                               \
  }

#endif
