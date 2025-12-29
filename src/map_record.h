#ifndef MAP_RECORD_H
#define MAP_RECORD_H

template <typename K, typename V> struct MapRecord {
  K key;
  V value;
  MapRecord(K k, V v) : key(k), value(v) {}
};

#define OKREC(t, t1, t2, v1, v2)                                               \
  ((Result<t<t1, t2> >){.val = new t<t1, t2>(v1, v2), .err = ""})

#define ERREC(t, t1, t2, e) ((Result<t<t1, t2> >){.val = NULL, .err = e})

#define TRYREC(t, t1, t2, v, r)                                                \
  if (!(r).err.empty()) {                                                      \
    return (Result<t<t1, t2> >){.val = reinterpret_cast<t<t1, t2> *>((r).val), \
                               .err = (r).err};                                \
  } else {                                                                     \
    v = (r).val;                                                               \
  }

#define TRYFREC(t, t1, t2, v, r, f)                                            \
  if (!(r).err.empty()) {                                                      \
    f return (Result<t<t1, t2> >){                                             \
        .val = reinterpret_cast<t<t1, t2> *>((r).val), .err = (r).err};        \
  } else {                                                                     \
    v = (r).val;                                                               \
  }

#endif
