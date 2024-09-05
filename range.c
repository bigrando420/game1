
typedef struct Range1f {
  float min;
  float max;
} Range1f;
// ...

typedef struct Range2f {
  Vector2 min;
  Vector2 max;
} Range2f;

inline Range2f range2f_make(Vector2 min, Vector2 max) { return (Range2f) { min, max }; }

Range2f range2f_shift(Range2f r, Vector2 shift) {
  r.min = v2_add(r.min, shift);
  r.max = v2_add(r.max, shift);
  return r;
}

Range2f range2f_make_center_center(Vector2 pos, Vector2 size) {
  return (Range2f){v2_add(pos, v2_mulf(size, -0.5)), v2_add(pos, v2_mulf(size, 0.5))};
}

// ?????
Range2f range2f_make_bottom_center(Vector2 size) {
  Range2f range = {0};
  range.max = size;
  range = range2f_shift(range, v2(size.x * -0.5, 0.0));
  return range;
}

Vector2 range2f_size(Range2f range) {
  Vector2 size = {0};
  size = v2_sub(range.min, range.max);
  size.x = fabsf(size.x);
  size.y = fabsf(size.y);
  return size;
}

bool range2f_contains(Range2f range, Vector2 v) {
  return v.x >= range.min.x && v.x <= range.max.x && v.y >= range.min.y && v.y <= range.max.y;
}

Vector2 range2f_get_center(Range2f r) {
	return (Vector2) { (r.max.x - r.min.x) * 0.5 + r.min.x, (r.max.y - r.min.y) * 0.5 + r.min.y };
}

Range2f range2f_make_bottom_left(Vector2 pos, Vector2 size) {
  return (Range2f){pos, v2_add(pos, size)};
}

Range2f range2f_make_top_right(Vector2 pos, Vector2 size) {
  return (Range2f){v2_sub(pos, size), pos};
}

Range2f range2f_make_bottom_right(Vector2 pos, Vector2 size) {
  return (Range2f){v2(pos.x-size.x, pos.y), v2(pos.x, pos.y+size.y)};
}

Range2f range2f_make_center_right(Vector2 pos, Vector2 size) {
  return (Range2f){v2(pos.x-size.x, pos.y-size.y*0.5), v2(pos.x, pos.y+size.y*0.5)};
}