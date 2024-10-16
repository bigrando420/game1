// The entry function is at the bottom of this file.
// Go there if you want to read this codebase.
// C needs declarations to be ordered, and header files are a waste of time. So reading this top to bottom isn't recommended.

#include "fmod/fmod_errors.h"
#include "fmod/fmod.h"
#include "fmod/fmod_studio.h"

#include "range.c"
#include "config.c"
#include "easings.c"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

// got this from ryan fleury's codebase, https://www.rfleury.com/ (originally called DeferLoop)
#define defer_scope(start, end) for(int _i_ = ((start), 0); _i_ == 0; _i_ += 1, (end))

#define scope_z_layer(Z) defer_scope(push_z_layer_in_frame(Z, current_draw_frame), pop_z_layer_in_frame(current_draw_frame))

Vector2 get_random_v2() {
	Vector2 vec = v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1));
	vec = v2_normalize(vec);
	return vec;
}

inline float64 fmod_cycling(float64 x, float64 y) {
	float remainder = x - (floorf(x/y) * y);
	return remainder;
}

inline int extract_sign(float a) {
	return a == 0 ? 0 : (a < 0 ? -1 : 1);
}

inline float v2_dist(Vector2 a, Vector2 b) {
  return fabsf(v2_length(v2_sub(a, b)));
}

#define m4_identity m4_make_scale(v3(1, 1, 1))

Range2f m4_transform_range2f(Matrix4 m, Range2f r) {
	r.min = m4_transform(m, v4(r.min.x, r.min.y, 0, 1)).xy;
	r.max = m4_transform(m, v4(r.max.x, r.max.y, 0, 1)).xy;
	return r;
}

typedef enum Direction {
	DIR_east,
	DIR_south,
	DIR_west,
	DIR_north,
} Direction;

typedef enum Pivot {
	PIVOT_bottom_left,
	PIVOT_bottom_center,
	PIVOT_bottom_right,
	PIVOT_center_left,
	PIVOT_center_center,
	PIVOT_center_right,
	PIVOT_top_left,
	PIVOT_top_center,
	PIVOT_top_right,
} Pivot;

Vector2 get_pivot_scale(Pivot pivot) {
	Vector2 pivot_mul;
	switch (pivot) {
		case PIVOT_bottom_left: pivot_mul = v2(0.0, 0.0); break;
		case PIVOT_bottom_center: pivot_mul = v2(0.5, 0.0); break;
		case PIVOT_bottom_right: pivot_mul = v2(1.0, 0.0); break;
		case PIVOT_center_left: pivot_mul = v2(0.0, 0.5); break;
		case PIVOT_center_center: pivot_mul = v2(0.5, 0.5); break;
		case PIVOT_center_right: pivot_mul = v2(1.0, 0.5); break;
		case PIVOT_top_center: pivot_mul = v2(0.5, 1.0); break;
		case PIVOT_top_left: pivot_mul = v2(0.0, 1.0); break;
		case PIVOT_top_right: pivot_mul = v2(1.0, 1.0); break;
	}
	return pivot_mul;
}

// ^^^ engine changes

// the scuff zone

// 0.2 means it has a 20% chance of returning true
bool pct_chance(float pct) {
	return get_random_float32_in_range(0, 1) < pct;
}

float float_alpha(float x, float min, float max) {
	float res = (x-min) / (max-min);
	res = clamp(res, 0.0, 1.0);
	return res;
}

// :utils

bool get_random_bool() {
	// this needs to be 0 -> 9 because just doing 0 -> 1 gives a repeating pattern...
	s64 b = get_random_int_in_range(0, 9);
	return b < 5;
}

int get_random_sign() {
	return (get_random_int_in_range(0, 1) == 0 ? -1 : 1);
}

// randy: it's sometimes easier to think about the inverse of min & max
#define clamp_bottom(a, b) max(a, b)
#define clamp_top(a, b) min(a, b)

// 0 -> 1
float sin_breathe(float time, float rate) {
	return (sin(time * rate) + 1.0) / 2.0;
}

bool v4_equals(Vector4 a, Vector4 b) {
 return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

bool v2i_equals(Vector2i a, Vector2i b) {
 return a.x == b.x && a.y == b.y;
}

bool almost_equals(float a, float b, float epsilon) {
 return fabs(a - b) <= epsilon;
}

bool animate_f32_to_target_with_epsilon(float* value, float target, float delta_t, float rate, float epsilon) {
	*value += (target - *value) * (1.0 - pow(2.0f, -rate * delta_t));
	if (almost_equals(*value, target, epsilon))
	{
		*value = target;
		return true; // reached
	}
	return false;
}
bool animate_f32_to_target(float* value, float target, float delta_t, float rate) {
	return animate_f32_to_target_with_epsilon(value, target, delta_t, rate, 0.001f);
}

void animate_v2_to_target(Vector2* value, Vector2 target, float delta_t, float rate) {
	animate_f32_to_target(&(value->x), target.x, delta_t, rate);
	animate_f32_to_target(&(value->y), target.y, delta_t, rate);
}

bool animate_f32_to_target_linear_with_epsilon(float* value, float target, float delta_t, float rate, float epsilon) {
	float delta = rate * delta_t;
	if (fabsf(target - *value) <= epsilon) {
		*value = target;
		return true; // Reached the target
	}
	if (*value < target) {
		*value += delta;
		if (*value > target) {
			*value = target;
		}
	} else {
		*value -= delta;
		if (*value < target) {
			*value = target;
		}
	}
	return false;
}

Range2f quad_to_range(Draw_Quad quad) {
	return (Range2f){quad.bottom_left, quad.top_right};
}

// ^^^ generic utils

// :col
// these get inited on startup because we don't have a #run for the hex_to_rgba(0x2a2d3aff) lol
Vector4 col_golden;
Vector4 col_fire;
Vector4 col_select;
Vector4 color_0;
Vector4 col_oxygen;
Vector4 col_tether;
Vector4 col_exp;
#define COLOR_GRAY v4(0.5, 0.5, 0.5, 1.0)

// :tweaks
float o2_fuel_length = 30.f;
float max_cam_shake_translate = 200.0f;
float max_cam_shake_rotate = 4.0f;
float player_reach_radius = 50.0f;
float tether_connection_radius = 80.0;
float o2_full_tank_deplete_length = 16.0f; // #volatile length of oxygen riser sfx
float oxygen_regen_tick_length = 0.01;
float oxygen_deplete_tick_length = 1.f;
const int tile_width = 8;
const float cursor_selection_slop_radius = 16.0f;
const float player_pickup_radius = 32.0f;
const int ice_vein_health = 10;
const int grass_health = 3;
const int flint_depo_health = 10;
const int exp_vein_health = 10;
const int copper_health = 10;
const int rock_health = 10;
const int tree_health = 10;

// :layers
typedef enum Layers {
	layer_background = 5,
	layer_world = 10,
	layer_buildings,
	layer_player,
	layer_ui = 20,
	layer_tooltip,
	layer_cursor_item,
} Layers;

// global :app stuff
// we could move this into an AppState struct.
// or we could just keep cavemaning it like this lol, since it's not needed.
Draw_Frame* current_draw_frame = 0;
Gfx_Shader_Extension global_shader = {0};
u64 frame_count = 0;
float exp_error_flash_alpha = 0;
float exp_error_flash_alpha_target = 0;
float camera_trauma = 0;
float camera_zoom = 5.3;
Vector2 camera_pos = {0};
float64 delta_t;
Gfx_Font* font;
u32 font_height_beeg = 128;
u32 font_height = 48;
u32 font_height_body = 36;

typedef struct AppFrame {
	bool connected_to_tether;
} AppFrame;
AppFrame app_frame = {0};
AppFrame last_app_frame = {0};

#pragma pack(push, 16)
typedef struct PointLight {
	Vector2 position; // xy, zw unused
	float radius;
	float intensity;
	Vector4 color;
} PointLight;

// :shader
#define POINT_LIGHT_MAX 256
typedef struct ShaderConstBuffer {
	float night_alpha;
	Vector3 pad1;
	PointLight point_lights[POINT_LIGHT_MAX];
	int point_light_count;
} ShaderConstBuffer;
#pragma pack(pop)
ShaderConstBuffer cbuffer = {0};

// #volatile
void set_col_override(Draw_Quad* q, Vector4 col_override) {
	q->userdata[0] = col_override;
}

// #volatile with shader
#define QUAD_TYPE_portal 1
// note, this couldn't be bitflags, because trying to use a float to store flags is error prone...
void set_quad_type(Draw_Quad* q, float type) {
	q->userdata[1].x = type;
}

int world_pos_to_tile_pos(float world_pos) {
	return floorf(world_pos / (float)tile_width);
}

float tile_pos_to_world_pos(int tile_pos) {
	return ((float)tile_pos * (float)tile_width);
}

Vector2i v2_world_pos_to_tile_pos(Vector2 world_pos) {
	return (Vector2i) { world_pos_to_tile_pos(world_pos.x), world_pos_to_tile_pos(world_pos.y) };
}

Vector2 v2_tile_pos_to_world_pos(Vector2i tile_pos) {
	return (Vector2) { tile_pos_to_world_pos(tile_pos.x), tile_pos_to_world_pos(tile_pos.y) };
}

Vector2 round_v2_to_tile(Vector2 world_pos) {
	world_pos.x = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.x));
	world_pos.y = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.y));
	return world_pos;
}

typedef struct Sprite {
	Gfx_Image* image;
	int frames;
} Sprite;
typedef enum SpriteID {
	SPRITE_nil,
	// SPRITE_player,
	SPRITE_tree0,
	SPRITE_tree1,
	SPRITE_rock0,
	SPRITE_item_rock,
	SPRITE_item_pine_wood,
	SPRITE_furnace,
	SPRITE_workbench,
	SPRITE_research_station,
	SPRITE_exp,
	SPRITE_exp_vein,
	SPRITE_copper_depo,
	SPRITE_raw_copper,
	SPRITE_copper_ingot,
	SPRITE_fiber,
	SPRITE_flint,
	SPRITE_flint_axe,
	SPRITE_flint_depo,
	SPRITE_flint_pickaxe,
	SPRITE_flint_scythe,
	SPRITE_grass,
	SPRITE_coal,
	SPRITE_oxygenerator,
	SPRITE_tether,
	SPRITE_o2_shard,
	SPRITE_ice_vein,
	SPRITE_player_walk,
	SPRITE_player_idle,
	SPRITE_ice_tile,
	SPRITE_burner_drill,
	SPRITE_longboi_test,
	SPRITE_coal_depo,
	SPRITE_raw_iron,
	SPRITE_iron_ingot,
	SPRITE_iron_depo,
	SPRITE_wall,
	SPRITE_wall_gate,
	SPRITE_fabricator,
	SPRITE_o2_emitter,
	SPRITE_turret,
	SPRITE_bullet,
	SPRITE_enemy_nest,
	SPRITE_red_core,
	SPRITE_large_ice_vein,
	SPRITE_wood_crate,
	SPRITE_conveyor_up,
	SPRITE_conveyor_down,
	SPRITE_conveyor_left,
	SPRITE_conveyor_right,
	SPRITE_large_coal_depo,
	SPRITE_anti_meteor,
	SPRITE_portal_icon,
	SPRITE_portal_frame,
	SPRITE_large_iron_depo,
	SPRITE_extractor_north,
	SPRITE_extractor_south,
	SPRITE_extractor_east,
	SPRITE_extractor_west,
	SPRITE_thumper,
	SPRITE_rock_small,
	SPRITE_rock_medium,
	SPRITE_rock_large,
	SPRITE_meteorite_depo,
	SPRITE_raw_meteorite,
	SPRITE_meteorite_ingot,
	SPRITE_blank_tp_focus,
	SPRITE_charged_tp_focus,
	SPRITE_portal_controller,
	// :sprite
	SPRITE_MAX,
} SpriteID;
// randy: maybe we make this an X macro?? https://chatgpt.com/share/260222eb-2738-4d1e-8b1d-4973a097814d
Sprite sprites[SPRITE_MAX];
Sprite* get_sprite(SpriteID id) {
	if (id >= 0 && id < SPRITE_MAX) {
		Sprite* sprite = &sprites[id];
		if (sprite->image) {
			return sprite;
		} else {
			return &sprites[0];
		}
	}
	return &sprites[0];
}
Vector2 get_sprite_size(Sprite* sprite) {
	return (Vector2) { sprite->image->width, sprite->image->height };
}

typedef enum ArchetypeID {
	ARCH_nil = 0,
	ARCH_tree,
	ARCH_player,
	ARCH_furnace,
	ARCH_workbench,
	ARCH_research_station,
	ARCH_exp_vein,
	ARCH_copper_depo,
	ARCH_flint_depo,
	ARCH_grass,
	ARCH_oxygenerator,
	ARCH_tether,
	ARCH_ice_vein,
	ARCH_tile_resource,
	ARCH_burner_drill,
	ARCH_coal_depo,
	ARCH_iron_depo,
	ARCH_wall,
	ARCH_wall_gate,
	ARCH_o2_emitter,
	ARCH_enemy1,
	ARCH_turret,
	ARCH_meteor,
	ARCH_enemy_nest,
	ARCH_large_ice_vein,
	ARCH_wood_crate,
	ARCH_conveyor,
	ARCH_large_coal_depo,
	ARCH_anti_meteor,
	ARCH_portal,
	ARCH_large_iron_depo,
	ARCH_extractor,
	ARCH_thumper,
	ARCH_rock_small,
	ARCH_rock_medium,
	ARCH_rock_large,
	ARCH_meteorite_depo,
	// these were items
	ARCH_rock,
	ARCH_exp,
	ARCH_raw_copper,
	ARCH_copper_ingot,
	ARCH_fiber,
	ARCH_coal,
	ARCH_o2_shard,
	ARCH_raw_iron,
	ARCH_iron_ingot,
	ARCH_bullet,
	ARCH_red_core,
	ARCH_raw_meteorite,
	ARCH_meteorite_ingot,
	ARCH_tp_focus,
	//
	ARCH_portal_controller,
	// :arch
	ARCH_MAX,
} ArchetypeID;

typedef struct ItemInstanceData {
	ArchetypeID id;
	int amount;
	bool has_focus_target;
	Vector2 focus_target_pos;
} ItemInstanceData; 
const ItemInstanceData empty_item = {0};

typedef enum DamageType {
	DMG_nil,
	DMG_axe,
	DMG_pickaxe,
	DMG_sickle,
	// #extend_dmg_type_here
	DMG_MAX,
} DamageType;

typedef enum EntityState {
	STATE_nil,
	STATE_idle,
	STATE_resting,
	// :state
} EntityState;

typedef struct EntityHandle {
	int id;
	int index;
} EntityHandle;

typedef enum GameError {
	GAME_ERR_nil,
	GAME_ERR_no_room_in_destination,
	GAME_ERR_no_fuel,
	GAME_ERR_no_o2,
} GameError;

typedef struct Entity Entity; // needs forward declare
typedef struct EntityFrame {
	Entity** connected_to_tethers;
	bool is_powered;
	Vector2 input_axis;
	SpriteID functional_sprite_id;
	bool can_interact;
	Entity* target_en;
	bool is_creation;
	bool did_shoot;
	bool is_awake;
	GameError error;
	Vector2 last_pos;
	bool is_being_picked_up;
	ItemInstanceData* focus_item;
	// :frame
} EntityFrame;

typedef struct StorageSlot {
	ItemInstanceData item;
	bool output_only; // can only take items from here, can't put in
	int desired_item_count;
	ArchetypeID desired_items[4];
} StorageSlot;

typedef enum Dimension {
	DIM_first,
	DIM_second,
	DIM_MAX,
} Dimension;

typedef struct Entity {
	bool is_valid;
	int id;
	ArchetypeID arch;
	ItemInstanceData item;
	Vector2 pos;
	bool render_sprite;
	SpriteID sprite_id;
	int health;
	int max_health;
	bool destroyable_world_item;
	int current_crafting_amount;
	float64 crafting_end_time;
	ItemInstanceData drops[4];
	int drops_count;
	DamageType dmg_type;
	ArchetypeID selected_crafting_item;
	int oxygen;
	int oxygen_max;
	float64 oxygen_deplete_end_time;
	float64 oxygen_regen_end_time;
	bool is_oxygen_tether;
	Vector2 tether_connection_offset;
	bool isnt_a_tile;
	bool right_click_remove;
	float health_bar_current_alpha; // note how caveman it feels to store this here lol. OOGA BOOGA
	bool has_physics;
	Vector2 velocity;
	Vector2 acceleration;
	float friction;
	bool disable_friction;
	float64 pick_up_cooldown_end_time;
	float white_flash_current_alpha;
	int exp_amount;
	StorageSlot storage_slots[8];
	int storage_slot_count;

	// deprecated
	ItemInstanceData input0;
	ItemInstanceData input1;
	ItemInstanceData output0;
	//

	int anim_index;
	float64 time_til_next_frame;
	Vector2i last_move_dir;
	float64 next_hit_end_time;
	float radius;
	bool interactable_entity;
	int last_fuel_max;
	int current_fuel;
	bool offset_based_on_tile_height;
	ArchetypeID current_crafting_item;
	int progress_on_crafting; // this goes up to 100 (deprecated)
	int progress; // use this now
	int progress_max;
	float64 next_crafting_progress_tick_end_time;
	bool has_collision;
	Range2f collision_bounds;
	bool ignore_collision;
	bool wall_seal;
	bool move_based_on_input_axis;
	float move_speed;
	bool is_being_knocked_back;
	float64 movement_cooldown_end_time;
	// EntityState state; // not needed yet
	bool is_agro;
	bool is_enemy;
	Vector2 last_shoot_dir;
	float rotation_current;
	float rotation_target;
	bool enemy_target;
	bool destroyable_by_explosion;
	bool meteor_destroy_without_drops;
	EntityHandle spawned_from;
	ArchetypeID big_resource_drop;
	s32 z_layer;
	bool has_input_storage;
	Direction dir;
	float64 next_update_end_time;
	bool o2_consume;
	int o2_consume_amount;
	float o2_consume_rate;
	float64 next_consume_end_time;
	bool has_anti_meteor_radius;
	Gfx_Image* render_target_image;
	Vector2 portal_view_pos;
	float64 teleported_at_time;
	bool is_item;
	bool can_be_placed;
	Dimension dim;
	Dimension dimension_target;
	u64 random_seed;
	bool flip_sprite;
	// :entity

	// state that is completely constant, derived by archetype
	// this was originally in a separate data structure, but it feels better inside here.
	// even though it's a "waste of memory" since these are constant across all entities.
	Vector2i tile_size;
	string pretty_name;
	//


	// copied from the old ItemData struct
	string description;
	SpriteID icon;
	int extra_axe_dmg; // #extend_dmg_type_here
	int extra_pickaxe_dmg;
	int extra_sickle_dmg;
	ArchetypeID for_structure;
	float craft_length;
	ArchetypeID furnace_transform_into;
	bool disabled;
	bool used_in_turret;
	// merged in from building.
	int exp_cost;
	ItemInstanceData ingredients[8];
	int ingredients_count;
	ItemInstanceData research_ingredients[4];
	int research_ingredients_count;
	int stack_size;

	// #continualsound
	// FMOD_STUDIO_EVENTINSTANCE* continual_sound;

	EntityFrame frame;
	EntityFrame last_frame;
} Entity;
#define MAX_ENTITY_COUNT 1024

//
// NOTE about the entity structure
//
// Separating out items, buildings, the player, or really anything from this megastruct is a bad idea.
//
// On the surface, it seems like we'd be making things more clean and compact. But it actually just leads to
// me needing to go to 17 different places in the codebase when adding in new pieces of content.
//
// Now that ALL THE DATA - runtime, constant, item or building specific - is contained with the Entity... The setup functions are
// the one-stop shop for defining the data... me likey!
//
// Memory efficient? No.
// Gameplay efficient? Yes.
//
// Memory or performance is not the bottlneck of making a good game.
// And if it ever *does* become a bottlneck. Measure it, then optimise.
//
// The iteration speed of adding in new content is 10000x more important.
//
// Game design comes 1st.
//

Entity entity_archetype_data[ARCH_MAX] = {0};
Entity get_archetype_data(ArchetypeID id) {
	return entity_archetype_data[id];
}
string get_archetype_pretty_name(ArchetypeID id) {
	return get_archetype_data(id).pretty_name;
}

SpriteID get_icon_from_arch_id(ArchetypeID id) {
	Entity data = get_archetype_data(id);
	return data.icon ? data.icon : data.sprite_id;
}
SpriteID get_sprite_id_from_item(ArchetypeID id) {
	return get_icon_from_arch_id(id);
}

Entity get_item_data(ArchetypeID id) {
	return get_archetype_data(id);
}
SpriteID get_sprite_id_from_item_instance(ItemInstanceData item) {
	if (item.id == ARCH_tp_focus) {
		return (item.has_focus_target ? SPRITE_charged_tp_focus : SPRITE_blank_tp_focus);
	}
	return get_icon_from_arch_id(item.id);
}

typedef enum UXState {
	UX_nil,
	UX_inventory,
	UX_place_mode,
	UX_workbench,
	UX_respawn,
	UX_entity_interaction,
	// :ux
	UX_MAX,
} UXState;

typedef struct UnlockState {
	bool is_known; // this'll be true when we discover the recipes
	u8 research_progress; // 0% -> 100%
} UnlockState;
bool is_fully_unlocked(UnlockState unlock_state) {
	return unlock_state.research_progress >= 100;
}

typedef Vector2i Tile;

typedef enum BiomeID {
	BIOME_void,
	BIOME_core,
	BIOME_barren,
	BIOME_forest,
	BIOME_copper,
	BIOME_copper_heavy,
	BIOME_ice,
	BIOME_ice_heavy,
	BIOME_iron,
	BIOME_enemy_nest,
	// :biome #volatile
	BIOME_MAX,
} BiomeID;

typedef struct WorldResourceData {
	BiomeID biome_id;
	Dimension dim;
	ArchetypeID arch_id;
	int dist_from_self;
	bool respawn;
} WorldResourceData;
// NOTE - trying out a new pattern here. That way we don't have to keep writing up enums to index into these guys. If we need dynamic runtime data, just make an array with the count of this array and have it essentially share the index. Like what I've done below in the world state.
WorldResourceData world_resources[] = {
	// { .biome_id=BIOME_forest, .arch_id=ARCH_flint_depo, .dist_from_self=20 },.respawn=

	{ .biome_id=BIOME_barren, .arch_id=ARCH_rock_small, .dist_from_self=10, .respawn=false },
	{ .biome_id=BIOME_barren, .arch_id=ARCH_rock_medium, .dist_from_self=15, .respawn=false },
	{ .biome_id=BIOME_barren, .arch_id=ARCH_rock_large, .dist_from_self=20, .respawn=false },
	{ .biome_id=BIOME_barren, .arch_id=ARCH_coal_depo, .dist_from_self=20, .respawn=false },

	{ .biome_id=BIOME_copper, .arch_id=ARCH_copper_depo, .dist_from_self=10, .respawn=false },

	{ .biome_id=BIOME_copper_heavy, .arch_id=ARCH_copper_depo, .dist_from_self=4, .respawn=false },
	// { .biome_id=BIOME_copper_heavy, .arch_id=ARCH_flint_depo, .dist_from_self=20 },.respawn=
	{ .biome_id=BIOME_copper_heavy, .arch_id=ARCH_rock, .dist_from_self=10, .respawn=false },

	{ .biome_id=BIOME_ice, .arch_id=ARCH_ice_vein, .dist_from_self=6, .respawn=true },
	{ .biome_id=BIOME_ice_heavy, .arch_id=ARCH_large_ice_vein, .dist_from_self=10, .respawn=false },

	{ .biome_id=BIOME_iron, .arch_id=ARCH_iron_depo, .dist_from_self=6, .respawn=true },

	{ .biome_id=BIOME_enemy_nest, .arch_id=ARCH_enemy_nest, .dist_from_self=4, .respawn=false },

	// second dim
	{ .dim=DIM_second, .biome_id=BIOME_forest, .arch_id=ARCH_tree, .dist_from_self=5, .respawn=false },
	{ .dim=DIM_second, .biome_id=BIOME_forest, .arch_id=ARCH_grass, .dist_from_self=3, .respawn=false },

	// :spawn_res system
};

typedef struct Map {
	int width;
	int height;
	BiomeID* tiles;
	Tile* biome_tiles[BIOME_MAX];
} Map;
Map maps[DIM_MAX] = {0};

u32 biome_colors[BIOME_MAX] = {
	0x000000, // void
	0xffffff, // core,
	0x484848, // barren,
	0x4db133, // forest,
	0xbf6937, // copper,
	0xa54b18, // copper heavy
	0x70d6cd, // ice
	0xa4eee6, // ice heavy
	0xe49f9f, // iron
	0xd31f1f, // enemy nest
	// :biome #volatile
};

Vector4 biome_col_hex_to_rgba(u32 col) {
	u8 r = (col>>16) & 0x000000FF;
	u8 g = (col>>8) & 0x000000FF;
	u8 b = (col>>0) & 0x000000FF;
	return (Vector4){r/255.0, g/255.0, b/255.0, 1};
}

inline Tile local_map_to_world_tile(Vector2i local, Dimension dim) {
	int x_index = local.x - floor((float)maps[dim].width * 0.5);
	int y_index = local.y - floor((float)maps[dim].height * 0.5);
	return (Tile){x_index, y_index};
}

Vector2i world_tile_to_local_map(Tile world, Dimension dim) {
	int x_index = world.x + floor((float)maps[dim].width * 0.5);
	int y_index = world.y + floor((float)maps[dim].height * 0.5);
	x_index = clamp(x_index, 0, maps[dim].width-1);
	y_index = clamp(y_index, 0, maps[dim].height-1);
	return (Vector2i){x_index, y_index};
}

inline int local_map_pos_to_index(Vector2i local_map, Dimension dim) {
	return local_map.y * maps[dim].width + local_map.x;
}

// :map
void init_biome_maps() {
	for (Dimension dim = 0; dim < DIM_MAX; dim++) {
		string path = tprint("res/sprites/dim%i_map.png", dim);;

		Map* map = &maps[dim];

		string png;
		bool ok = os_read_entire_file(path, &png, get_heap_allocator());
		assert(ok);

		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		third_party_allocator = get_heap_allocator();
		u8* stb_data = stbi_load_from_memory(png.data, png.count, &width, &height, &channels, STBI_rgb_alpha);
		assert(stb_data);
		assert(channels == 4);
		third_party_allocator = ZERO(Allocator);

		map->width = width;
		map->height = height;
		map->tiles = alloc(get_heap_allocator(), width * height * sizeof(BiomeID));

		bool found_biomes[BIOME_MAX] = {0};

		for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			int index = y * width + x;
			u8* pixel_first_channel = stb_data + index * channels;
			u32 pixel_color =
				(pixel_first_channel[0]) << 24 | // r
				(pixel_first_channel[1]) << 16 | // g
				(pixel_first_channel[2]) << 8 | // b
				(pixel_first_channel[3]) << 0; // a

			u32 pixel_no_alpha = pixel_color >> 8;

			for (BiomeID i = 0; i < ARRAY_COUNT(biome_colors); i++) {
				if (biome_colors[i] == pixel_no_alpha) {
					map->tiles[index] = i;
					found_biomes[i] = true;
				}
			}
		}

		for (BiomeID i = 0; i < BIOME_MAX; i++) {
			if (!found_biomes[i]) {
				log_warning("Biome %i is unused", i);
			}

			Tile* tiles;
			growing_array_init_reserve((void**)&tiles, sizeof(Tile), 128, get_heap_allocator());

			for (int y = 0; y < map->height; y++)
			for (int x = 0; x < map->width; x++)
			{
				BiomeID biome_at_tile = map->tiles[local_map_pos_to_index(v2i(x, y), dim)];
				if (biome_at_tile == i) {
					Tile t = local_map_to_world_tile(v2i(x, y), dim);
					growing_array_add((void**)&tiles, &t);
				}
			}

			map->biome_tiles[i] = tiles;
		}
	}
}

BiomeID biome_at_tile(Tile tile, Dimension dim) {
	BiomeID biome = 0;
	int x_index = tile.x + floor((float)maps[dim].width * 0.5);
	int y_index = tile.y + floor((float)maps[dim].height * 0.5);
	if (x_index < maps[dim].width && x_index >= 0 && y_index < maps[dim].height && y_index >= 0) {
		biome = maps[dim].tiles[y_index * maps[dim].width + x_index];
	}
	return biome;
}

#define INV_COUNT 24

typedef struct World {
	int id_count;
	u64 tick_count; 
	u64 day_count;
	float64 cycle_end_time;
	float64 time_elapsed;
	Entity entities[MAX_ENTITY_COUNT];
	ItemInstanceData inventory_items[INV_COUNT];
	UXState ux_state;
	float inventory_alpha;
	float inventory_alpha_target;
	float building_alpha;
	float building_alpha_target;
	ArchetypeID placing_building;
	EntityHandle interacting_with_entity;
	ArchetypeID selected_research_thing;
	UnlockState item_unlocks[ARCH_MAX];
	float64 resource_next_spawn_end_time[ARRAY_COUNT(world_resources)];
	EntityHandle oxygenerator;
	ItemInstanceData mouse_cursor_item;
	float night_alpha;
	float night_alpha_target;
	float64 next_close_meteor_spawn_end_time;
	float64 next_far_meteor_spawn_end_time;
	Direction cursor_rotate_dir;
	// :world :state
} World;
World* world = 0;

typedef struct TileEntityCache TileEntityCache; // forward declr

typedef struct WorldFrame {
	int render_target_w;
	int render_target_h;
	Entity* selected_entity;
	Vector2 camera_pos_copy; // this is kinda jank, but it's here so we can pass down the camera position to the rendering, instead of extracting it from the view matrix
	Matrix4 world_proj;
	Matrix4 world_view;
	Matrix4 screen_proj;
	Matrix4 screen_view;
	bool hover_consumed;
	bool show_inventory;
	Entity* player;
	TileEntityCache* tile_entity_caches[DIM_MAX];
	bool is_creation;
	bool draw_portals;
	// WORLD :frame state
} WorldFrame;
WorldFrame world_frame;

inline Entity* get_nil_entity() {
	return &world->entities[0];
}
bool is_nil(Entity* en) {
	return en == get_nil_entity();
}
bool is_valid(Entity* en) {
	return !is_nil(en) && en->is_valid;
}

EntityHandle handle_from_entity(Entity* en) {
	s64 index = en - world->entities;
	return (EntityHandle){ en->id, index };
}
Entity* entity_from_handle(EntityHandle handle) {
	Entity* en = &world->entities[handle.index];
	if (en->id == handle.id) {
		return en;
	} else {
		return get_nil_entity();
	}
}

Entity* get_player() {
	return world_frame.player;
}

// I <3 C
#include "fmod_sound.c"

// top level :func dump

Range2f get_camera_view_rect_in_world_space() {
	Range2f rect;
	rect.min = v2(-1, -1);
	rect.max = v2(1, 1);
	rect = m4_transform_range2f(m4_inverse(world_frame.world_proj), rect);
	rect = m4_transform_range2f(world_frame.world_view, rect);
	return rect;
}

bool is_player_alive() {
	return get_player()->health > 0;
}

// :defaults
void entity_apply_defaults(Entity* en) {
	en->tile_size = v2i(1, 1);
	en->stack_size = 64;
}

Entity* entity_create(Dimension dim) {
	Entity* entity_found = 0;
	for (int i = 1; i < MAX_ENTITY_COUNT; i++) {
		Entity* existing_entity = &world->entities[i];
		if (!existing_entity->is_valid) {
			entity_found = existing_entity;
			break;
		}
	}
	assert(entity_found, "No more free entities!");
	entity_found->is_valid = true;
	entity_apply_defaults(entity_found);

	world->id_count += 1;
	entity_found->id = world->id_count;
	entity_found->frame.is_creation = true;
	entity_found->dim = dim;
	entity_found->random_seed = get_random();

	return entity_found;
}

void entity_zero_immediately(Entity* en) {
	if (en->render_target_image) {
		delete_image(en->render_target_image);
	}
	memset(en, 0, sizeof(Entity));
}

void entity_max_health_setter(Entity* en, int new_max_health) {

	if (en->frame.is_creation) {
		// fresh entity, just setup as normal
		en->health = new_max_health;
		en->max_health = new_max_health;
	} else {
		// we're deserialising.

		float prev_max_health = en->max_health;
		en->max_health = new_max_health;

		if (prev_max_health == en->health || en->health >= new_max_health) {
			en->health = new_max_health;
		}
	}
}

// :item :setup

void setup_rock(Entity* en) {
	en->arch = ARCH_rock;
	en->pretty_name = STR("Rock");
	en->icon = SPRITE_item_rock;
}

void setup_raw_copper(Entity* en) {
	en->arch = ARCH_raw_copper;
	en->pretty_name = STR("Raw Copper");
	en->icon=SPRITE_raw_copper;
	en->furnace_transform_into=ARCH_copper_ingot;
}
void setup_copper_ingot(Entity* en) {
	en->arch = ARCH_copper_ingot;
	en->pretty_name = STR("Copper Ingot");
	en->icon=SPRITE_copper_ingot;
}
void setup_fiber(Entity* en) {
	en->arch = ARCH_fiber;
	en->pretty_name = STR("Fiber");
	en->icon=SPRITE_fiber;
}
void setup_coal(Entity* en) {
	en->arch = ARCH_coal;
	en->pretty_name = STR("Coal");
	en->icon=SPRITE_coal;
}
void setup_o2_shard(Entity* en) {
	en->arch = ARCH_o2_shard;
	en->pretty_name = STR("Oxygen Shard");
	en->icon=SPRITE_o2_shard;
}
void setup_raw_iron(Entity* en) {
	en->arch = ARCH_raw_iron;
	en->pretty_name = STR("Raw Iron");
	en->icon=SPRITE_raw_iron;
	en->furnace_transform_into=ARCH_iron_ingot;
}
void setup_iron_ingot(Entity* en) {
	en->arch = ARCH_iron_ingot;
	en->pretty_name = STR("Iron Ingot");
	en->icon=SPRITE_iron_ingot;
}
void setup_bullet(Entity* en) {
	en->arch = ARCH_bullet;
	en->pretty_name = STR("Bullet");
	en->icon=SPRITE_bullet;
	en->disabled = true; 
}
void setup_red_core(Entity* en) {
	en->arch = ARCH_red_core;
	en->pretty_name = STR("Æ█Ξ2vX Core");
	en->icon=SPRITE_red_core;
}
void setup_raw_meteorite(Entity* en) {
	en->arch = ARCH_raw_meteorite;
	en->pretty_name = STR("Raw Meteorite");
	en->icon=SPRITE_raw_meteorite;
	en->furnace_transform_into=ARCH_meteorite_ingot;
}
void setup_meteorite_ingot(Entity* en) {
	en->arch = ARCH_meteorite_ingot;
	en->pretty_name = STR("Meteorite Ingot");
	en->icon=SPRITE_meteorite_ingot;
}
void setup_tp_focus(Entity* en) {
	en->arch = ARCH_tp_focus;
	en->pretty_name = STR("Quantum Lens");
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=100};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_iron_ingot, .amount=20};
	en->stack_size=1;
	en->sprite_id = SPRITE_charged_tp_focus;
}

void setup_exp(Entity* en) {
	en->arch = ARCH_exp;

	// in-world thingo
	en->exp_amount = 1;
	en->ignore_collision = true;

	en->pretty_name = STR("Experience");
	en->icon = SPRITE_exp;
	en->stack_size = 999;
}

// :setup world things

void setup_meteorite_depo(Entity* en) {
	en->pretty_name = STR("Meteorite Deposit");
	en->arch = ARCH_meteorite_depo;
	en->sprite_id = SPRITE_meteorite_depo;
	entity_max_health_setter(en, rock_health * 3);
	en->destroyable_world_item = true;
	en->drops_count = 2;
	en->drops[0] = (ItemInstanceData){.id=ARCH_rock, .amount=3};
	en->drops[1] = (ItemInstanceData){.id=ARCH_raw_meteorite, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->tile_size = v2i(3, 1);
	en->has_collision = true;
}

void setup_rock_small(Entity* en) {
	en->pretty_name = STR("Rock");
	en->arch = ARCH_rock_small;
	en->sprite_id = SPRITE_rock_small;
	entity_max_health_setter(en, rock_health);
	en->destroyable_world_item = true;
	en->destroyable_by_explosion = true;
	en->meteor_destroy_without_drops = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_rock, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->tile_size = v2i(2, 1);
	en->has_collision = true;
}
void setup_rock_medium(Entity* en) {
	en->pretty_name = STR("Rock");
	en->arch = ARCH_rock_medium;
	en->destroyable_by_explosion = true;
	en->meteor_destroy_without_drops = true;
	en->sprite_id = SPRITE_rock_medium;
	entity_max_health_setter(en, rock_health * 1.5);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_rock, .amount=2};
	en->dmg_type = DMG_pickaxe;
	en->tile_size = v2i(2, 2);
	en->has_collision = true;
}
void setup_rock_large(Entity* en) {
	en->pretty_name = STR("Rock");
	en->arch = ARCH_rock_large;
	en->destroyable_by_explosion = true;
	en->meteor_destroy_without_drops = true;
	en->sprite_id = SPRITE_rock_large;
	entity_max_health_setter(en, rock_health * 2);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_rock, .amount=3};
	en->dmg_type = DMG_pickaxe;
	en->tile_size = v2i(3, 2);
	en->has_collision = true;
}

void setup_large_iron_depo(Entity* en) {
	en->pretty_name = STR("Large Iron Deposit");
	en->arch = ARCH_large_iron_depo;
	en->sprite_id = SPRITE_large_iron_depo;
	en->big_resource_drop = ARCH_raw_iron;
	entity_max_health_setter(en, 1);
	en->has_collision = true;
	en->tile_size = v2i(3, 2);
}

// :setup :buildings

// :thumper
void setup_thumper(Entity* en) {
	en->pretty_name = STR("Thumper");
	en->arch = ARCH_thumper;
	en->sprite_id = SPRITE_thumper;
	en->interactable_entity = true;
	entity_max_health_setter(en, 10);
	en->has_collision = true;
	en->tile_size = v2i(2, 2);
	en->radius = tile_width * 10;

	en->can_be_placed = true;
	en->icon = SPRITE_thumper;
	en->description = STR("Burns fuel to slam into the ground and destroy resources in a radius.");
	en->research_ingredients_count=1;
	en->research_ingredients[0] = (ItemInstanceData){.id=ARCH_exp, .amount=100};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_iron_ingot, .amount=10};
}

// :qcontroller
void setup_portal_controller(Entity* en) {
	en->arch = ARCH_portal_controller;
	en->tile_size = v2i(1, 1);
	en->pretty_name = STR("Quantum Controller");
	en->description = STR("Place next to a Quantum Gate to control where it teleports to.");
	en->sprite_id = SPRITE_portal_controller;
	en->has_collision=true;
	en->interactable_entity=true;

	en->storage_slot_count = 1;
	en->storage_slots[0].desired_item_count = 1;
	en->storage_slots[0].desired_items[0] = ARCH_tp_focus;

	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=100};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_iron_ingot, .amount=10};
}
// :portal
void setup_portal(Entity* en) {
	en->arch = ARCH_portal;
	en->tile_size = v2i(9, 3);
	en->pretty_name = STR("Quantum Gate");
	en->render_target_image = 0;
	en->interactable_entity = true;
	en->sprite_id = SPRITE_portal_frame;
	en->offset_based_on_tile_height=true;

	en->icon=SPRITE_portal_icon;
	en->description=STR("Quantum transportation. Use at own risk.");
	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=500};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_iron_ingot, .amount=100};
}

void setup_anti_meteor(Entity* en) {
	en->pretty_name = STR("Meteor Defense");
	en->arch = ARCH_anti_meteor;
	en->sprite_id = SPRITE_anti_meteor;
	en->tile_size = v2i(2, 1);
	entity_max_health_setter(en, 5);
	en->has_collision = true;
	en->enemy_target = true;
	en->o2_consume = true;
	en->o2_consume_rate = 1.f;
	en->o2_consume_amount = 1;
	en->is_oxygen_tether = true;
	en->has_anti_meteor_radius = true;
	en->radius = tile_width * 8;

	en->description=STR("Redirects meteors in a radius");
	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=50};
	en->ingredients_count=2;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_meteorite_ingot, .amount=2};
	en->ingredients[1]=(ItemInstanceData){.id=ARCH_iron_ingot, .amount=2};
}

void setup_large_coal_depo(Entity* en) {
	en->pretty_name = STR("Large Coal Deposit");
	en->arch = ARCH_large_coal_depo;
	en->sprite_id = SPRITE_large_coal_depo;
	en->big_resource_drop = ARCH_coal;
	entity_max_health_setter(en, 1);
	en->has_collision = true;
	en->tile_size = v2i(3, 2);
}

void setup_extractor(Entity* en) {
	en->arch = ARCH_extractor;
	en->pretty_name = STR("Extractor");
	en->has_input_storage = true;
	en->sprite_id = SPRITE_extractor_east;

	en->disabled=true;
	en->can_be_placed = true;
	// 	.disabled=true, 
	// .to_build=ARCH_extractor,
	// .icon=SPRITE_extractor_east,
	// .description=STR("Extracts items from the thing beside it"),
	// .research_ingredients_count=1,
	// .research_ingredients={{.id=ARCH_exp, .amount=50}},
	// .ingredients_count=1,
	// .ingredients={ {.id=ARCH_iron_ingot, .amount=1} }
}

void setup_conveyor(Entity* en) {
	en->arch = ARCH_conveyor;
	en->pretty_name = STR("Conveyor");
	en->has_input_storage = true;
	en->sprite_id = SPRITE_conveyor_right;

	en->disabled=true;
	en->can_be_placed = true;
	// 	.disabled=true,
	// .to_build=ARCH_conveyor,
	// .icon=SPRITE_conveyor_right,
	// .description=STR("Moves items"),
	// .research_ingredients_count=1,
	// .research_ingredients={{.id=ARCH_exp, .amount=50}},
	// .ingredients_count=1,
	// .ingredients={ {.id=ARCH_iron_ingot, .amount=1} }
}

void setup_wood_crate(Entity* en) {
	en->arch = ARCH_wood_crate;
	en->sprite_id = SPRITE_wood_crate;
	en->pretty_name = STR("Storage Crate");
	en->has_collision = true;
	en->interactable_entity = true;
	en->has_input_storage = true;

	en->description=STR("Stores a single item with an infinite stack size.");
	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=20};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_rock, .amount=4};
}

void setup_large_ice_vein(Entity* en) {
	en->pretty_name = STR("Large Ice Vein");
	en->arch = ARCH_large_ice_vein;
	en->sprite_id = SPRITE_large_ice_vein;
	en->big_resource_drop = ARCH_o2_shard;
	entity_max_health_setter(en, 1);
	en->has_collision = true;
	en->tile_size = v2i(3, 2);
}

// :nest
void setup_enemy_nest(Entity* en) {
	en->arch = ARCH_enemy_nest;
	en->sprite_id = SPRITE_enemy_nest;
	en->tile_size = v2i(2, 2);
}

void setup_meteor(Entity* en) {
	en->arch = ARCH_meteor;
	en->radius = 70.f;
}

void setup_turret(Entity* en) {
	en->arch = ARCH_turret;
	en->pretty_name = STR("Turret");
	en->tile_size = v2i(2, 2);
	en->destroyable_by_explosion = true;
	en->has_collision = true;
	en->sprite_id = SPRITE_turret;
	en->interactable_entity = true;
	en->enemy_target = true;
	entity_max_health_setter(en, 10);

	en->disabled=true;
	en->can_be_placed = true;
		// item_data[ARCH_turret] = (Entity){
		// 	.disabled=true,
		// 	.to_build=ARCH_turret,
		// 	.icon=SPRITE_turret,
		// 	.description=STR("Shoot bullets at nearby enemies"),
		// 	.research_ingredients_count=1,
		// 	.research_ingredients={{.id=ARCH_exp, .amount=50}},
		// 	.ingredients_count=3,
		// 	.ingredients={ {.id=ARCH_iron_ingot, .amount=2}, {.id=ARCH_red_core, .amount=1} }
		// };
}

// :enemy
Vector2 enemy_size = {8, 8};
void setup_enemy1(Entity* en) {
	en->arch = ARCH_enemy1;
	en->pretty_name = STR("Æ█Ξ2vX");
	en->destroyable_by_explosion = true;
	en->destroyable_world_item = true;
	en->has_physics = true;
	en->collision_bounds = range2f_make_center_center(v2(0, 0), enemy_size);
	en->is_enemy = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_red_core, .amount=1};
	entity_max_health_setter(en, 20);
}

void setup_o2_emitter(Entity* en) {
	en->arch = ARCH_o2_emitter;
	en->pretty_name = STR("Oxygen Feeder");
	en->is_oxygen_tether = true;
	en->tile_size = v2i(1, 1);
	en->destroyable_by_explosion = true;
	en->sprite_id = SPRITE_o2_emitter;
	en->has_collision = true;
	en->collision_bounds = range2f_make_center_center(v2(0, 0), v2(tile_width, tile_width));
	en->wall_seal = true;
	en->enemy_target = true;
	entity_max_health_setter(en, 3);

	en->description=STR("Feed oxygen into a sealed room with 3x more efficency");
	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=100};
	en->ingredients_count=2;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_iron_ingot, .amount=5};
	en->ingredients[1]=(ItemInstanceData){.id=ARCH_o2_shard, .amount=10};
}

void setup_wall(Entity* en) {
	en->arch = ARCH_wall;
	en->pretty_name = STR("Wall");
	en->tile_size = v2i(1, 1);
	en->destroyable_by_explosion = true;
	en->sprite_id = SPRITE_wall;
	en->has_collision = true;
	en->collision_bounds = range2f_make_center_center(v2(0, 0), v2(tile_width, tile_width));
	en->wall_seal = true;

	en->description=STR("Can be used to build a sealed Oxygen room");
	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=50};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_rock, .amount=1};
}
void setup_wall_gate(Entity* en) {
	en->arch = ARCH_wall_gate;
	en->pretty_name = STR("Wall Gate");
	en->destroyable_by_explosion = true;
	en->tile_size = v2i(1, 1);
	en->sprite_id = SPRITE_wall_gate;
	en->wall_seal = true;

	en->description=STR("Allows travel into a sealed room");
	en->can_be_placed = true;
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=50};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_rock, .amount=1};
}

void setup_iron_depo(Entity* en) {
	en->arch = ARCH_iron_depo;
	en->pretty_name = STR("Iron Deposit");
	en->tile_size = v2i(2, 1);
	// en->destroyable_by_explosion = true;
	en->sprite_id = SPRITE_iron_depo;
	entity_max_health_setter(en, 10);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_raw_iron, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_coal_depo(Entity* en) {
	en->arch = ARCH_coal_depo;
	en->pretty_name = STR("Coal Deposit");
	en->tile_size = v2i(2, 1);
	// en->destroyable_by_explosion = true;
	en->sprite_id = SPRITE_coal_depo;
	entity_max_health_setter(en, 10);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_coal, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

// :burner 
void setup_burner_drill(Entity* en) {
	en->arch = ARCH_burner_drill;
	en->pretty_name = STR("Burner Drill");
	en->tile_size = v2i(1, 1);
	en->destroyable_by_explosion = true;
	en->sprite_id = SPRITE_burner_drill;
	en->interactable_entity = true;
	en->has_collision = true;
	en->enemy_target = true;
	entity_max_health_setter(en, 5);

	en->disabled=true;
	en->can_be_placed = true;
		// 	item_data[ARCH_burner_drill] = (Entity){
		// 	.disabled=true,
		// 	.to_build=ARCH_burner_drill,
		// 	.icon=SPRITE_burner_drill,
		// 	.description=STR("Burns coal to drill into large veins"),
		// 	.research_ingredients_count=1,
		// 	.research_ingredients={{.id=ARCH_exp, .amount=200}},
		// 	.ingredients_count=2,
		// 	.ingredients={ {.id=ARCH_iron_ingot, .amount=4}, {.id=ARCH_rock, .amount=4} }
		// };
}

void setup_tile_resource(Entity* en) {
	en->arch = ARCH_tile_resource;
	en->sprite_id = SPRITE_ice_tile;
}

void setup_ice_vein(Entity* en) {
	en->pretty_name = STR("Oxygen Vein");
	en->arch = ARCH_ice_vein;
	en->sprite_id = SPRITE_ice_vein;
	// en->destroyable_by_explosion = true;
	entity_max_health_setter(en, ice_vein_health);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_o2_shard, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_tether(Entity* en) {
	en->arch = ARCH_tether;
	en->pretty_name = STR("Tether");
	en->destroyable_by_explosion = false; // turning this off for now so it's not annoying. maybe we want this in the early game tho, with some kind of more durable upgrade later on ?
	en->sprite_id = SPRITE_tether;
	en->is_oxygen_tether = true;
	en->tether_connection_offset.y = 4;

	en->can_be_placed = true;
	en->description=STR("Extends oxygen range");
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=50};
	en->ingredients_count=2;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_meteorite_ingot, .amount=1};
	en->ingredients[1]=(ItemInstanceData){.id=ARCH_fiber, .amount=8};
}

void setup_oxygenerator(Entity* en) {
	en->arch = ARCH_oxygenerator;
	en->has_anti_meteor_radius = true;
	en->radius = tile_width * 8;
	en->has_input_storage = true;
	en->pretty_name = STR("Emergency Oxygenerator");
	en->sprite_id = SPRITE_oxygenerator;
	en->is_oxygen_tether = true;
	en->interactable_entity = true;
	en->has_collision = true;
	en->collision_bounds = range2f_make_center_center(v2(0, 0), v2(tile_width, tile_width));
	en->oxygen_max = 40;
	en->oxygen = en->oxygen_max;
}
bool do_oxygenerator_error(Entity* en) {
	return en->oxygen < en->oxygen_max * 0.5 && en->input0.amount <= 0;
}

void setup_grass(Entity* en) {
	en->pretty_name = STR("Grass");
	en->arch = ARCH_grass;
	en->sprite_id = SPRITE_grass;
	// en->destroyable_by_explosion = true;
	entity_max_health_setter(en, grass_health);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_fiber, .amount=get_random_int_in_range(1, 2)};
	en->dmg_type = DMG_sickle;
}

void setup_flint_depo(Entity* en) {
	en->pretty_name = STR("Flint Deposit");
	en->arch = ARCH_flint_depo;
	en->tile_size = v2i(2, 1);
	en->sprite_id = SPRITE_flint_depo;
	// en->destroyable_by_explosion = true;
	entity_max_health_setter(en, flint_depo_health);
	en->destroyable_world_item = true;
	// en->drops_count = 1;
	// en->drops[0] = (ItemInstanceData){.id=ARCH_flint, .amount=get_random_int_in_range(1, 2)};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_copper_depo(Entity* en) {
	en->pretty_name = STR("Copper Deposit");
	en->arch = ARCH_copper_depo;
	en->sprite_id = SPRITE_copper_depo;
	// en->destroyable_by_explosion = true;
	entity_max_health_setter(en, copper_health);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_raw_copper, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_exp_vein(Entity* en) {
	en->arch = ARCH_exp_vein;
	en->sprite_id = SPRITE_exp_vein;
	entity_max_health_setter(en, exp_vein_health);
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemInstanceData){.id=ARCH_exp, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_pickaxe;
}

void setup_furnace(Entity* en) {
	en->arch = ARCH_furnace;
	en->tile_size = v2i(2, 2);
	en->enemy_target = true;
	en->destroyable_by_explosion = true;
	en->pretty_name = STR("Furnace");
	en->sprite_id = SPRITE_furnace;
	en->interactable_entity = true;
	en->has_collision = true;
	entity_max_health_setter(en, 3);

	en->can_be_placed = true;
	en->description=STR("Can burn stuff into something more useful.");
	en->research_ingredients_count=1;
	en->research_ingredients[0]=(ItemInstanceData){.id=ARCH_exp, .amount=20};
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_rock, .amount=5};

	en->storage_slot_count = 3;

	en->storage_slots[0].desired_item_count = 2;
	en->storage_slots[0].desired_items[0] = ARCH_coal;

	{
		// put all furance transform items into the desired slot 1
		StorageSlot* slot = &en->storage_slots[1];
		slot->desired_item_count = 0;
		for (ArchetypeID i = 0; i < ARCH_MAX; i++) {
			Entity item_data = get_item_data(i);
			if (item_data.furnace_transform_into) {
				assert(slot->desired_item_count < ARRAY_COUNT(slot->desired_items));
				slot->desired_items[slot->desired_item_count] = i;
				slot->desired_item_count += 1;
			}
		}
	}

	en->storage_slots[2].output_only = true;
}

void setup_workbench(Entity* en) {
	en->arch = ARCH_workbench;
	en->pretty_name = STR("Fabricator");
	en->sprite_id = SPRITE_fabricator;
	en->interactable_entity = true;
	en->tile_size = v2i(2,2);
	en->has_collision = true;

	// fabricator not craftable atm
	en->disabled=true;
	en->can_be_placed = true;

		// 	item_data[ARCH_workbench] = (Entity){
		// 	.disabled=true,
		// 	.to_build=ARCH_workbench,
		// 	.icon=SPRITE_fabricator,
		// 	.description=STR("Crafts things."),
		// 	.ingredients_count=2,
		// 	.ingredients={ {.id=ARCH_pine_wood, .amount=10}, {.id=ARCH_fiber, .amount=5} }
		// };
}

void setup_research_station(Entity* en) {
	en->arch = ARCH_research_station;
	en->tile_size = v2i(2, 1);
	en->pretty_name = STR("Research Station");
	en->sprite_id = SPRITE_research_station;
	en->interactable_entity = true;
	en->has_collision = true;

	en->can_be_placed = true;
	en->description=STR("Research recipes to unlock more buildings."),
	en->ingredients_count=1;
	en->ingredients[0]=(ItemInstanceData){.id=ARCH_rock, .amount=5};
}

void setup_player(Entity* en) {
	en->arch = ARCH_player;
	en->z_layer = layer_player;
	en->enemy_target = true;
	en->destroyable_by_explosion = true;
	en->move_based_on_input_axis = true;
	en->move_speed = 70.f;
	entity_max_health_setter(en, 1);
	en->oxygen_max = 17;
	en->oxygen = en->oxygen_max;
	en->has_physics = true;
	en->friction = 20.f;
	en->collision_bounds = range2f_make_center_center(v2(0,0), v2(6, 7));
}

void setup_tree(Entity* en) {
	en->arch = ARCH_tree;
	en->pretty_name = STR("Pine Tree");
	// en->destroyable_by_explosion = true;
	en->tile_size = v2i(2, 2);
	en->offset_based_on_tile_height = true;
	entity_max_health_setter(en, tree_health);
	en->destroyable_world_item = true;
	// en->drops_count = 1;
	// en->drops[0] = (ItemInstanceData){.id=ARCH_pine_wood, .amount=1};
	en->dmg_type = DMG_axe;
	en->has_collision = true;
	en->collision_bounds = range2f_make_bottom_center(v2(4, 4));
	en->collision_bounds = range2f_shift(en->collision_bounds, v2(0, -tile_width));

	u64 prev_seed = seed_for_random;
	seed_for_random = en->random_seed;
	en->flip_sprite = get_random_bool();
	en->sprite_id = SPRITE_tree0 + get_random_bool();
	seed_for_random = prev_seed;
}

void entity_setup(Entity* en, ArchetypeID id) {
	switch (id) {
		// filling these in so we can get compiler errors for missing cases
		case ARCH_nil: break;
		case ARCH_MAX: break;
		//
		case ARCH_exp: setup_exp(en); break;
		case ARCH_player: setup_player(en); break;
		case ARCH_furnace: setup_furnace(en); break;
		case ARCH_workbench: setup_workbench(en); break;
		case ARCH_research_station: setup_research_station(en); break;
		case ARCH_rock: setup_rock(en); break;
		case ARCH_tree: setup_tree(en); break;
		case ARCH_exp_vein: setup_exp_vein(en); break;
		case ARCH_copper_depo: setup_copper_depo(en); break;
		case ARCH_flint_depo: setup_flint_depo(en); break;
		case ARCH_grass: setup_grass(en); break;
		case ARCH_oxygenerator: setup_oxygenerator(en); break;
		case ARCH_tether: setup_tether(en); break;
		case ARCH_ice_vein: setup_ice_vein(en); break;
		case ARCH_tile_resource: setup_tile_resource(en); break;
		case ARCH_burner_drill: setup_burner_drill(en); break;
		case ARCH_coal_depo: setup_coal_depo(en); break;
		case ARCH_iron_depo: setup_iron_depo(en); break;
		case ARCH_wall: setup_wall(en); break;
		case ARCH_wall_gate: setup_wall_gate(en); break;
		case ARCH_o2_emitter: setup_o2_emitter(en); break;
		case ARCH_enemy1: setup_enemy1(en); break;
		case ARCH_turret: setup_turret(en); break;
		case ARCH_meteor: setup_meteor(en); break;
		case ARCH_enemy_nest: setup_enemy_nest(en); break;
		case ARCH_large_ice_vein: setup_large_ice_vein(en); break;
		case ARCH_wood_crate: setup_wood_crate(en); break;
		case ARCH_conveyor: setup_conveyor(en); break;
		case ARCH_large_coal_depo: setup_large_coal_depo(en); break;
		case ARCH_anti_meteor: setup_anti_meteor(en); break;
		case ARCH_portal: setup_portal(en); break;
		case ARCH_large_iron_depo: setup_large_iron_depo(en); break;
		case ARCH_extractor: setup_extractor(en); break;
		case ARCH_thumper: setup_thumper(en); break;
		case ARCH_rock_small: setup_rock_small(en); break;
		case ARCH_rock_medium: setup_rock_medium(en); break;
		case ARCH_rock_large: setup_rock_large(en); break;
		case ARCH_meteorite_depo: setup_meteorite_depo(en); break;
		case ARCH_portal_controller: setup_portal_controller(en); break;
		case ARCH_raw_copper: setup_raw_copper(en); break;
		case ARCH_copper_ingot: setup_copper_ingot(en); break;
		case ARCH_fiber: setup_fiber(en); break;
		case ARCH_coal: setup_coal(en); break;
		case ARCH_o2_shard: setup_o2_shard(en); break;
		case ARCH_raw_iron: setup_raw_iron(en); break;
		case ARCH_iron_ingot: setup_iron_ingot(en); break;
		case ARCH_bullet: setup_bullet(en); break;
		case ARCH_red_core: setup_red_core(en); break;
		case ARCH_raw_meteorite: setup_raw_meteorite(en); break;
		case ARCH_meteorite_ingot: setup_meteorite_ingot(en); break;
		case ARCH_tp_focus: setup_tp_focus(en); break;
		// :arch :setup
	}

	assert(en->arch, "Archetype not setup in function.");
}

void setup_item_with_instance(Entity* en, ItemInstanceData item) {
	entity_setup(en, item.id);
	en->is_item = true;
	en->item = item;
	en->has_collision = false;
}
void setup_item(Entity* en, ArchetypeID id) {
	entity_setup(en, id);
	en->is_item = true;
	en->item.id = id;
	if (en->item.amount == 0) {
		en->item.amount = 1;
	}
	en->has_collision = false;
}

void setup_entity_archetype_data_cache() {
	for (ArchetypeID i = 1; i < ARCH_MAX; i++) {
		Entity* en = &entity_archetype_data[i];
		entity_apply_defaults(en);
		entity_setup(en, i);
	}
}

Vector2 get_mouse_pos_in_ndc() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
	float window_w = window.width;
	float window_h = window.height;

	// Normalize the mouse coordinates
	float ndc_x = (mouse_x / (window_w * 0.5f)) - 1.0f;
	float ndc_y = (mouse_y / (window_h * 0.5f)) - 1.0f;

	return (Vector2){ ndc_x, ndc_y };
}

Vector2 get_mouse_pos_in_current_space() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;

	// this is a bit icky, but we're using the draw frame's matricies if valid. If not, we default to world space
	Matrix4 proj;
	if (current_draw_frame) {
		proj = current_draw_frame->projection;
	} else {
		proj = world_frame.world_proj;
	}
	Matrix4 view;
	if (current_draw_frame) {
		view = current_draw_frame->camera_xform;
	} else {
		view = world_frame.world_view;
	}

	float window_w = window.width;
	float window_h = window.height;

	// Normalize the mouse coordinates
	float ndc_x = (mouse_x / (window_w * 0.5f)) - 1.0f;
	float ndc_y = (mouse_y / (window_h * 0.5f)) - 1.0f;

	// Transform to world coordinates
	Vector4 world_pos = v4(ndc_x, ndc_y, 0, 1);
	world_pos = m4_transform(m4_inverse(proj), world_pos);
	world_pos = m4_transform(view, world_pos);
	// log("%f, %f", world_pos.x, world_pos.y);

	// Return as 2D vector
	return (Vector2){ world_pos.x, world_pos.y };
}

float screen_width = 240.0;
float screen_height = 135.0;
// this is icky
void set_screen_space() {
	current_draw_frame->camera_xform = world_frame.screen_view;
	current_draw_frame->projection = world_frame.screen_proj;
}
void set_world_space() {
	current_draw_frame->projection = world_frame.world_proj;
	current_draw_frame->camera_xform = world_frame.world_view;
}

Draw_Quad* draw_sprite_in_rect_with_xform(SpriteID sprite_id, Range2f rect, Vector4 col, float pad_pct, Matrix4 xform) {
	Sprite* sprite = get_sprite(sprite_id);
	Vector2 sprite_size = get_sprite_size(sprite);

	// make it smoller (padding)
	{
		Vector2 size = range2f_size(rect);
		Vector2 offset = rect.min;
		rect = range2f_shift(rect, v2_mulf(rect.min, -1));
		rect.min.x += size.x * pad_pct * 0.5;
		rect.min.y += size.y * pad_pct * 0.5;
		rect.max.x -= size.x * pad_pct * 0.5;
		rect.max.y -= size.y * pad_pct * 0.5;
		rect = range2f_shift(rect, offset);
	}

	// this shrinks the rect if the sprite is too smol
	{
		Vector2 rect_size = range2f_size(rect);
		float size_diff_x = rect_size.x - sprite_size.x;
		if (size_diff_x < 0) {
			size_diff_x = 0;
		}

		float size_diff_y = rect_size.y - sprite_size.y;
		if (size_diff_y < 0) {
			size_diff_y = 0;
		}

		Vector2 offset = rect.min;
		rect = range2f_shift(rect, v2_mulf(rect.min, -1));
		rect.min.x += size_diff_x * 0.5;
		rect.min.y += size_diff_y * 0.5;
		rect.max.x -= size_diff_x * 0.5;
		rect.max.y -= size_diff_y * 0.5;
		rect = range2f_shift(rect, offset);
	}

	// ratio render lock
	if (sprite_size.x > sprite_size.y) { // long boi

		// height is a ratio of width
		Vector2 range_size = range2f_size(rect);
		rect.max.y = rect.min.y + (range_size.x * (sprite_size.y/sprite_size.x));
		// center along the Y
		float new_height = rect.max.y - rect.min.y;
		rect = range2f_shift(rect, v2(0, (range_size.y - new_height) * 0.5));

	} else if (sprite_size.y > sprite_size.x) { // tall boi
		
		// width is a ratio of height
		Vector2 range_size = range2f_size(rect);
		rect.max.x = rect.min.x + (range_size.y * (sprite_size.x/sprite_size.y));
		// center along the X
		float new_width = rect.max.x - rect.min.x;
		rect = range2f_shift(rect, v2((range_size.x - new_width) * 0.5, 0));
	}

	// apply xform
	rect = m4_transform_range2f(xform, rect);

	return draw_image_in_frame(sprite->image, rect.min, range2f_size(rect), col, current_draw_frame);
}

// pad_pct just shrinks the rect by a % of itself ... 0.2 is a nice default
Draw_Quad* draw_sprite_in_rect(SpriteID sprite_id, Range2f rect, Vector4 col, float pad_pct) {
	return draw_sprite_in_rect_with_xform(sprite_id, rect, col, pad_pct, m4_identity);
}

inline float64 now() {
	return world->time_elapsed;
}
inline float64 app_now() {
	return os_get_elapsed_seconds();
}

float alpha_from_end_time(float64 end_time, float length) {
	return float_alpha(now(), end_time-length, end_time);
}

bool has_reached_end_time(float64 end_time) {
	return now() > end_time;
}

Tile* get_tile_list_at_pos_based_on_arch(Vector2 pos, ArchetypeID id) {
	Entity en_data = get_archetype_data(id);

	Tile* tiles;
	growing_array_init_reserve((void**)&tiles, sizeof(Tile), 4, get_temporary_allocator());

	Tile start_tile = v2_world_pos_to_tile_pos(pos);

	int half_width = en_data.tile_size.x / 2;
	int half_height = en_data.tile_size.y / 2;
	for (int y = -half_height; y < half_height + en_data.tile_size.y % 2; y++)
	for (int x = -half_width; x < half_width + en_data.tile_size.x % 2; x++) {
		Tile tile = {start_tile.x + x, start_tile.y + y};
		growing_array_add((void**)&tiles, &tile);
	}

	return tiles;
}

Vector2 v2_tile_pos_to_entity_world_pos(Vector2i tile_pos, ArchetypeID id) {
	Vector2 pos = v2_tile_pos_to_world_pos(tile_pos);
	pos.x += get_archetype_data(id).tile_size.x * tile_width * 0.5;
	pos.y += get_archetype_data(id).tile_size.y * tile_width * 0.5;
	return pos;
}

typedef struct TileCache {
	Tile world_tile;
	Entity* entity;
	bool visited;
} TileCache;
typedef struct TileEntityCache {
	TileCache* tiles;
	int tile_count;
} TileEntityCache;

void add_new_entity_to_tile_cache(Entity* en, Dimension dim) {
	TileEntityCache* cache = world_frame.tile_entity_caches[dim];

	Tile* tiles = get_tile_list_at_pos_based_on_arch(en->pos, en->arch);
	for (int j=0;j<growing_array_get_valid_count(tiles);j++) {
		Tile tile = tiles[j];
		Vector2i local_tile_pos = world_tile_to_local_map(tile, dim);
		int index = local_tile_pos.y * maps[dim].width + local_tile_pos.x;
		if (index >= 0 && index < cache->tile_count) {
			cache->tiles[index].entity = en;
		}
	}
}

void create_tile_entity_pair_cache() {
	for (Dimension dim = 0; dim < DIM_MAX; dim++) {
		if (world_frame.tile_entity_caches[dim] != 0) {
			continue;
		}

		tm_scope("allocs")
		{
			world_frame.tile_entity_caches[dim] = alloc(get_temporary_allocator(), sizeof(TileEntityCache));
			TileEntityCache* cache = world_frame.tile_entity_caches[dim];
			cache->tile_count = maps[dim].height * maps[dim].width;
			cache->tiles = alloc(get_temporary_allocator(), sizeof(TileCache) * cache->tile_count);
		}

		TileEntityCache* cache = world_frame.tile_entity_caches[dim];

		tm_scope("store tiles")
		for (int y = 0; y < maps[dim].height; y++)
		for (int x = 0; x < maps[dim].width; x++) {
			TileCache* tc = &cache->tiles[local_map_pos_to_index(v2i(x, y), dim)];
			tc->world_tile = local_map_to_world_tile(v2i(x, y), dim);
		}

		tm_scope("add enttiy to tile cache")
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			add_new_entity_to_tile_cache(en, dim);
		}
	}
}

TileCache* tile_cache_at_tile(Tile tile, Dimension dim) {
	TileEntityCache* cache = world_frame.tile_entity_caches[dim];
	Vector2i local_tile = world_tile_to_local_map(tile, dim);
	int index = local_tile.y * maps[dim].width + local_tile.x;
	if (index < 0 || index > cache->tile_count) {
		return 0;
	} else {
		return &cache->tiles[index];
	}
}

Entity* entity_at_tile(Tile tile, Dimension dim) {
	TileEntityCache* cache = world_frame.tile_entity_caches[dim];
	Vector2i local_tile = world_tile_to_local_map(tile, dim);
	int index = local_tile.y * maps[dim].width + local_tile.x;
	if (index < 0 || index > cache->tile_count) {
		return 0;
	} else {
		return cache->tiles[index].entity;
	}
}

Gfx_Text_Metrics draw_text_with_pivot(Gfx_Font *font, string text, u32 raster_height, Vector2 position, Vector2 scale, Vector4 color, Pivot pivot) {
	Gfx_Text_Metrics metrics = measure_text(font, text, raster_height, scale);
	position = v2_sub(position, metrics.visual_pos_min);
	Vector2 pivot_mul = get_pivot_scale(pivot);
	position = v2_sub(position, v2_mul(metrics.visual_size, pivot_mul));
	draw_text_in_frame(font, text, raster_height, position, scale, color, current_draw_frame);
	return metrics;
}

// :func dump

Dimension get_player_dim() {
	return world_frame.player->dim;
}

bool move_item_instance_to_inv(ItemInstanceData* item) {
	Entity item_data = get_item_data(item->id);

	// First pass: Try to stack into existing items
	for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
		ItemInstanceData* inv_item = &world->inventory_items[i];

		if (inv_item->id == item->id) {
			int space_left = item_data.stack_size - inv_item->amount;
			if (space_left > 0) {
				int amount_to_add = (item->amount < space_left) ? item->amount : space_left;
				inv_item->amount += amount_to_add;
				item->amount -= amount_to_add;

				if (item->amount == 0) {
					*item = empty_item;
					return true;
				}
			}
		}
	}

	// Second pass: Try to find empty slots
	for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
		ItemInstanceData* inv_item = &world->inventory_items[i];
		if (inv_item->id == 0) {
			int amount_to_add = (item->amount < item_data.stack_size) ? item->amount : item_data.stack_size;
			*inv_item = *item;
			inv_item->amount = amount_to_add;
			item->amount -= amount_to_add;

			if (item->amount == 0) {
				*item = empty_item;
				return true;
			}
		}
	}

	assert(item->amount != 0);

	// couldn't add all amount to inventory
	return false;
}

// copied and modified from the one below.
bool can_add_item_to_inv(ItemInstanceData item) {
	Entity item_data = get_item_data(item.id);

	int amount_left = item.amount;

	// First pass: Try to stack into existing items
	for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
		ItemInstanceData* inv_item = &world->inventory_items[i];

		if (inv_item->id == item.id) {
			int space_left = item_data.stack_size - inv_item->amount;
			if (space_left > 0) {
				int amount_to_add = (amount_left < space_left) ? amount_left : space_left;
				amount_left -= amount_to_add;

				if (amount_left == 0) {
					return true;
				}
			}
		}
	}

	// Second pass: Try to find empty slots
	for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
		ItemInstanceData* inv_item = &world->inventory_items[i];
		if (inv_item->id == 0) {
			int amount_to_add = (amount_left < item_data.stack_size) ? amount_left : item_data.stack_size;
			amount_left -= amount_to_add;

			if (amount_left == 0) {
				return true;
			}
		}
	}

	bool can_add_some = amount_left != item.amount;
	return can_add_some;
}

bool attempt_add_item_to_inv(ItemInstanceData item) {
	Entity item_data = get_item_data(item.id);

	int amount_left = item.amount;

	// First pass: Try to stack into existing items
	for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
		ItemInstanceData* inv_item = &world->inventory_items[i];

		if (inv_item->id == item.id) {
			int space_left = item_data.stack_size - inv_item->amount;
			if (space_left > 0) {
				int amount_to_add = (amount_left < space_left) ? amount_left : space_left;
				inv_item->amount += amount_to_add;
				amount_left -= amount_to_add;

				if (amount_left == 0) {
					return true;
				}
			}
		}
	}

	// Second pass: Try to find empty slots
	for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
		ItemInstanceData* inv_item = &world->inventory_items[i];
		if (inv_item->id == 0) {
			int amount_to_add = (amount_left < item_data.stack_size) ? amount_left : item_data.stack_size;
			inv_item->id = item.id;
			inv_item->amount = amount_to_add;
			amount_left -= amount_to_add;

			if (amount_left == 0) {
				return true;
			}
		}
	}

	// If we still have amount_left, we couldn't add all items
	return false;
}

// :camera
Matrix4 construct_view_matrix(Vector2 pos, float zoom, float trauma) {

	float cam_shake = clamp_top(pow(trauma, 3), 1);

	Matrix4 view = m4_identity;

	view = m4_identity;

	// randy: these might be ordered incorrectly for the camera shake. Not sure.

	// translate into position
	view = m4_translate(view, v3(pos.x, pos.y, 0));

	// translational shake
	float shake_x = max_cam_shake_translate * cam_shake * get_random_float32_in_range(-1, 1);
	float shake_y = max_cam_shake_translate * cam_shake * get_random_float32_in_range(-1, 1);
	view = m4_translate(view, v3(shake_x, shake_y, 0));

	// rotational shake
	// float shake_rotate = max_cam_shake_rotate * cam_shake * get_random_float32_in_range(-1, 1);
	// view = m4_rotate_z(view, shake_rotate);

	// scale the zoom
	view = m4_scale(view, v3(1.0/zoom, 1.0/zoom, 1.0));

	return view;
}

void set_world_view() {
	world_frame.world_view = construct_view_matrix(camera_pos, camera_zoom, camera_trauma);
	world_frame.camera_pos_copy = camera_pos;
}

Vector2 ndc_pos_to_screen_pos(Vector2 ndc) {
	float w = world_frame.render_target_w;
	float h = world_frame.render_target_h;
	Vector2 screen = ndc;
	screen.x = (ndc.x * 0.5f + 0.5f) * w;
	screen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * h;
	return screen;
}

Vector2 world_pos_to_ndc(Vector2 world_pos) {

	Matrix4 proj;
	if (current_draw_frame) {
		proj = current_draw_frame->projection;
	} else {
		proj = world_frame.world_proj;
	}
	Matrix4 view;
	if (current_draw_frame) {
		view = current_draw_frame->camera_xform;
	} else {
		view = world_frame.world_view;
	}

	Matrix4 world_space_to_ndc = m4_identity;
	world_space_to_ndc = m4_mul(proj, m4_inverse(view));

	Vector2 ndc = m4_transform(world_space_to_ndc, v4(v2_expand(world_pos), 0, 1)).xy;
	return ndc;
}

float get_world_radius(Dimension dim) {
	return maps[dim].width * tile_width / 2;
}

// :spawn
float resource_respawn_length = 10.f;
void do_resource_respawning() {
	Dimension dim = DIM_first;

	for (int i = 0; i < ARRAY_COUNT(world_resources); i++) {
		WorldResourceData data = world_resources[i];
		if (!data.respawn) {
			continue;
		}

		Tile* potential_spawn_tiles;
		growing_array_init((void**)&potential_spawn_tiles, sizeof(Tile), get_temporary_allocator());
		for (int j = 0; j < growing_array_get_valid_count(maps[dim].biome_tiles[data.biome_id]); j++) {
			Tile t = maps[dim].biome_tiles[data.biome_id][j];
			TileCache* tc = tile_cache_at_tile(t, dim);
			if (!tc->entity) {
				growing_array_add((void**)&potential_spawn_tiles, &tc->world_tile);
			}
		}

		// find random spot ahead of time
		int iterations = 0;
		int count = growing_array_get_valid_count(potential_spawn_tiles);
		bool found_spot = false;
		Vector2 spawn_pos = {0};
		if (count) {
			while (true) {
				// pick from a random spot
				Tile spawn_tile = potential_spawn_tiles[get_random_int_in_range(0, count-1)];

				spawn_pos = v2_tile_pos_to_entity_world_pos(spawn_tile, data.arch_id);

				// is it close to any entities?
				bool too_close = false;
				for (int j = 0; j < MAX_ENTITY_COUNT; j++) {
					Entity* en = &world->entities[j];
					if (en->is_valid && en->arch == data.arch_id && !en->isnt_a_tile) {
						int tile_radius = data.dist_from_self;
						if (v2_dist(spawn_pos, en->pos) < tile_width * tile_radius) {
							too_close = true;
							break;
						}
					}
				}

				if (!too_close) {
					found_spot = true;
					break;
				}

				iterations += 1;
				if (iterations > 10) {
					break;
				}
			}
		}

		// plonk down
		if (found_spot) {
			if (world->resource_next_spawn_end_time[i] == 0) {
				world->resource_next_spawn_end_time[i] = now() + resource_respawn_length;
			}

			if (has_reached_end_time(world->resource_next_spawn_end_time[i])) {
				Entity* en = entity_create(dim);
				entity_setup(en, data.arch_id);
				en->pos = spawn_pos;
				add_new_entity_to_tile_cache(en, dim);

				world->resource_next_spawn_end_time[i] = 0;
			}
		}

	}
}

bool has_enough_for_recipe(ItemInstanceData* recipe, int count) {
	for (int i = 0; i < count; i++) {
		ItemInstanceData ing = recipe[i];
		Entity ing_data = get_item_data(ing.id);

		int total_count = 0;

		for (int j = 0; j < ARRAY_COUNT(world->inventory_items); j++) {
			ItemInstanceData* item = &world->inventory_items[j];
			if (item->id == ing.id && item->amount) {
				total_count += item->amount;
			}
		}

		if (total_count < ing.amount) {
			return false; 
		}
	}

	return true;
}

void consume_recipe(ItemInstanceData* recipe, int count) {
	for (int i = 0; i < count; i++) {
		ItemInstanceData ing = recipe[i];
		Entity ing_data = get_item_data(ing.id);

		int remaining_amount = ing.amount;

		for (int j = 0; j < ARRAY_COUNT(world->inventory_items); j++) {
			ItemInstanceData* item = &world->inventory_items[j];
			if (item->id == ing.id && item->amount) {
				item->amount -= remaining_amount;
				remaining_amount = 0;

				if (item->amount < 0) {
					remaining_amount = -item->amount;
					item->amount = 0;
				}

				if (item->amount == 0) {
					*item = (ItemInstanceData){0};
				}

				if (remaining_amount == 0) {
					break;
				}
			}
		}

		assert(remaining_amount == 0, "consume_recipe went tits up...");
	}
}

inline float get_error_flash_frequency() {
	return sin_breathe(os_get_elapsed_seconds(), 6.f);
}

Vector4 get_red_error_col_override() {
	return v4(1, 0, 0, 0.8 * get_error_flash_frequency());
}

Range2f get_world_rect(Dimension dim) {
	float width = maps[dim].width * tile_width;
	float height = maps[dim].height * tile_width;
	return (Range2f) {.min=v2(width * -0.5, height * -0.5), .max=v2(width * 0.5, height * 0.5)};
}

typedef struct ArchetypeWeight {
	ArchetypeID value;
	float weight;
} ArchetypeWeight;

ArchetypeID random_weighted_archetype(ArchetypeWeight* weights, int weights_count) {
	// Calculate the total weight
	float total_weight = 0.0f;
	for (int i = 0; i < weights_count; i++) {
		total_weight += weights[i].weight;
	}

	// Generate a random number in the range [0, total_weight)
	float random_number = get_random_float32_in_range(0.0f, total_weight);

	// Find the item that corresponds to the random number
	float cumulative_weight = 0.0f;
	for (int i = 0; i < weights_count; i++) {
		cumulative_weight += weights[i].weight;
		if (random_number < cumulative_weight) {
			return weights[i].value;
		}
	}

	return 0;
}

// 1.0 is medium intensity
void add_point_light(Vector2 world_pos, Vector4 col, float radius, float intensity) {
	if (cbuffer.point_light_count >= POINT_LIGHT_MAX) {
		static bool has_notified = false;
		if (!has_notified) {
			log_warning("Max point lights reached");
			has_notified = true;
		}
		return;
	}

	PointLight* pl = &cbuffer.point_lights[cbuffer.point_light_count];
	cbuffer.point_light_count += 1;

	pl->color = col;
	pl->intensity = intensity;
	pl->position = ndc_pos_to_screen_pos(world_pos_to_ndc(world_pos));

	{
		// Transform the light's position to screen space
    pl->position = ndc_pos_to_screen_pos(world_pos_to_ndc(world_pos));

    // Compute an offset position in world space
    Vector2 offset_world_pos = v2_add(world_pos, v2(radius, 0.0f));

    // Transform the offset position to screen space
    Vector2 offset_screen_pos = ndc_pos_to_screen_pos(world_pos_to_ndc(offset_world_pos));

    // Calculate the radius in screen space
    float screen_radius = v2_length(v2_sub(offset_screen_pos, pl->position));

		pl->radius = screen_radius;
	}
}

void shader_recompile() {
	string source;
	bool ok = os_read_entire_file("res/shader.hlsl", &source, get_heap_allocator());
	assert(ok);

	Gfx_Shader_Extension shader;
	ok = gfx_compile_shader_extension(source, sizeof(ShaderConstBuffer), &shader);
	if (ok) {
		global_shader = shader;
	} else {
		log_error("shader compile failed");
	}

	dealloc_string(get_heap_allocator(), source);
}

bool is_night() {
	return almost_equals(world->night_alpha, 1.0, 0.1);
}

Range2f get_entity_collision_bounds(Entity* en) {
	Range2f bounds = en->collision_bounds;
	if (almost_equals(range2f_size(bounds).x, 0, 0.1) || almost_equals(range2f_size(bounds).y, 0, 0.1)) {
		// auto-gen bounds from sprite size
		// bounds = range2f_make_center_center(v2(0,0), get_sprite_size(get_sprite(en->sprite_id)));

		// gen from tile size instead?
		bounds = range2f_make_center_center(v2(0,0), v2(en->tile_size.x * tile_width, en->tile_size.y * tile_width));
	}
	return bounds;
}

Vector2 snap_position_to_nearest_tile_based_on_arch(Vector2 pos, ArchetypeID id) {
	Vector2i tile_size = get_archetype_data(id).tile_size;

	if (tile_size.x % 2 == 0) {
		pos.x = roundf(pos.x / (float)tile_width) * tile_width;
	} else {
		pos.x = floorf(pos.x / (float)tile_width) * tile_width;
		pos.x += tile_width * 0.5;
	}

	if (tile_size.y % 2 == 0) {
		pos.y = roundf(pos.y / (float)tile_width) * tile_width;
	} else {
		pos.y = floorf(pos.y / (float)tile_width) * tile_width;
		pos.y += tile_width * 0.5;
	}

	return pos;
}

Vector2 get_offset_for_rendering(ArchetypeID id) {
	Entity en = get_archetype_data(id);

	Sprite* sprite = get_sprite(en.sprite_id);

	Vector2 sprite_size = get_sprite_size(sprite );

	Vector2 offset = {0};
	offset.x -= sprite_size.x * 0.5;

	if (en.offset_based_on_tile_height) {
		offset.y -= en.tile_size.y * tile_width * 0.5;
	} else {
		offset.y -= sprite_size.y * 0.5;
	}

	if (en.arch == ARCH_burner_drill) {
		offset.x = tile_width * -0.5;
	}

	return offset;
}

Range2f get_entity_range(Entity* en) {
	Vector2 bottom_left = v2_add(en->pos, get_offset_for_rendering(en->arch));
	return (Range2f){ bottom_left, v2_add(bottom_left, get_sprite_size(get_sprite(en->sprite_id))) };
}

void do_entity_exp_drops(Entity* en) {
	int exp_amount = 3;
	if (en->is_enemy) {
		exp_amount = 8;
	}
	for (int i = 0; i < get_random_int_in_range(exp_amount-1, exp_amount); i++) {
		Entity* orb = entity_create(en->dim);
		setup_item(orb, ARCH_exp);
		orb->is_item = true;
		orb->pos = en->pos;
		orb->has_physics = true;
		orb->friction = 20.f;
		orb->velocity = v2_normalize(v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1)));
		orb->velocity = v2_mulf(orb->velocity, get_random_float32_in_range(100, 200));
		orb->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.4, 0.6);
	}
}

void drop_item_at_pos(ItemInstanceData item, Vector2 pos, Dimension dim) {
	Entity* drop = entity_create(dim);
	setup_item_with_instance(drop, item);
	drop->pos = pos;
	drop->pos = v2_add(drop->pos, v2(get_random_float32_in_range(-2, 2), get_random_float32_in_range(-2, 2)));
	drop->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.1, 0.3);
}

void do_entity_drops(Entity* en) {
	ArchetypeID *drops;
	// purposefully making the reserve 2 items, to prove the resizing works, and that you don't have to worry about the size of the array.
	growing_array_init_reserve((void**)&drops, sizeof(ArchetypeID), 2, get_temporary_allocator());

	if (en->drops_count) {
		// drops from entity data
		for (int i = 0; i < en->drops_count; i++) {
			ItemInstanceData drop = en->drops[i];
			for (int j = 0; j < drop.amount; j++) {
				growing_array_add((void**)&drops, &drop.id);
			}
		}
	}

	if (en->right_click_remove) {

		// drop building stuff
		Entity item_data = get_item_data(en->arch);
		growing_array_add((void**)&drops, &en->arch);
		// not dropping the raw materials anymore, so it's more clear that the item got destroyed.
		// for (int i = 0; i < item_data.ingredients_count; i++) {
		// 	ItemInstanceData drop = item_data.ingredients[i];
		// 	for (int j = 0; j < drop.amount; j++) {
		// 		growing_array_add((void**)&drops, &drop.id);
		// 	}
		// }

		for (int j = 0; j < en->input0.amount; j++) {
			growing_array_add((void**)&drops, &en->input0.id);
		}

		for (int j = 0; j < en->input1.amount; j++) {
			growing_array_add((void**)&drops, &en->input1.id);
		}
	}

	// create all the item drops
	for (int i = 0; i < growing_array_get_valid_count(drops); i++) {
		ArchetypeID drop_id = drops[i];
		Entity* drop = entity_create(en->dim);
		setup_item(drop, drop_id);
		drop->pos = en->pos;
		drop->pos = v2_add(drop->pos, v2(get_random_float32_in_range(-2, 2), get_random_float32_in_range(-2, 2)));
		drop->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.1, 0.3);
	}
}

void draw_item_amount_in_rect(ItemInstanceData item_amount, Range2f rect) {
	draw_sprite_in_rect(get_sprite_id_from_item_instance(item_amount), rect, COLOR_WHITE, 0.1);

	float x0 = rect.max.x;
	float y0 = rect.min.y;
	x0 -= 1;
	y0 += 1;

	Vector2 text_scale = v2(0.1, 0.1);
	string txt = tprint("%i", item_amount.amount);
	draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_bottom_right);
}

void item_tooltip(ItemInstanceData item) {
	push_z_layer_in_frame(layer_tooltip, current_draw_frame);

	Vector2 text_scale = v2(0.1, 0.1);
	Entity item_data = get_item_data(item.id);

	Vector2 size = v2(40, 20);
	Vector2 pos = get_mouse_pos_in_current_space();
	pos.y -= size.y;

	draw_rect_in_frame(pos, size, COLOR_BLACK, current_draw_frame);

	float x0, y0;
	x0 = pos.x + size.x * 0.5;
	y0 = pos.y + size.y;
	y0 -= 2.f; // arbitrary padding

	Gfx_Text_Metrics met = draw_text_with_pivot(font, item_data.pretty_name, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
	y0 -= met.visual_size.y;
	y0 -= 2.f;

	if (item_data.description.count) {
		string txt = item_data.description;
		Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
		y0 -= met.visual_size.y;
		y0 -= 2.f;
	}

	if (item.id == ARCH_tp_focus) {
		string txt;
		if (item.has_focus_target) {
			txt = tprint("Target Position: x%i, y%i", (int)item.focus_target_pos.x, (int)item.focus_target_pos.y);
		} else {
			txt = STR("Left click to set position.");
		}
		Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
		y0 -= met.visual_size.y;
		y0 -= 2.f;
	}


	pop_z_layer_in_frame(current_draw_frame);
}

float max_trauma = 0.6;

void camera_shake(float amount) {
	camera_trauma += amount;
	if (camera_trauma > max_trauma) {
		camera_trauma = max_trauma;
	}
}

void camera_shake_at_pos(float amount, Vector2 source_position, float radius, float falloff_distance) {
	Vector2 cam_pos = camera_pos;

	// Calculate the distance between the camera and the source position
	float distance = v2_dist(cam_pos, source_position);

	if (distance <= radius) {
		// Within the radius, apply the full shake amount
		camera_shake(amount);
	} else if (distance <= radius + falloff_distance) {
		// Between radius and radius + falloff_distance, interpolate the shake amount linearly
		float t = (distance - radius) / falloff_distance;
		float adjusted_amount = amount * (1.0f - t);
		camera_shake(adjusted_amount);
	}
	// Beyond radius + falloff_distance, do not apply any shake
}

bool does_overlap_existing_entities(Vector2 pos, ArchetypeID id, Dimension dim) {

	Tile* tiles = get_tile_list_at_pos_based_on_arch(pos, id);
	for (int i = 0; i < growing_array_get_valid_count(tiles); i++) {
		Tile tile = tiles[i];
		TileCache* tc = tile_cache_at_tile(tile, dim);
		if (tc->entity) {
			return true;
		}
	}

	return false;
}

// :spawn on startup
void spawn_world_resources() {

	create_tile_entity_pair_cache();

	for (int dim = 0; dim < DIM_MAX; dim++)
	for (int i = 0; i < ARRAY_COUNT(world_resources); i++) {
		WorldResourceData data = world_resources[i];
		if (data.dim != dim) {
			continue;
		}

		Tile* potential_spawn_tiles;
		growing_array_init((void**)&potential_spawn_tiles, sizeof(Tile), get_temporary_allocator());
		for (int j = 0; j < growing_array_get_valid_count(maps[dim].biome_tiles[data.biome_id]); j++) {
			Tile t = maps[dim].biome_tiles[data.biome_id][j];
			TileCache* tc = tile_cache_at_tile(t, dim);
			if (!tc->entity) {
				growing_array_add((void**)&potential_spawn_tiles, &tc->world_tile);
			}
		}

		int iterations = 0;
		int count = growing_array_get_valid_count(potential_spawn_tiles);
		if (count) {
			while (true) {
				// pick from a random spot
				Tile spawn_tile = potential_spawn_tiles[get_random_int_in_range(0, count-1)];

				Vector2 spawn_pos = v2_tile_pos_to_entity_world_pos(spawn_tile, data.arch_id);

				// is it close to any entities?
				bool too_close = false;
				for (int j = 0; j < MAX_ENTITY_COUNT; j++) {
					Entity* en = &world->entities[j];
					if (en->is_valid && en->arch == data.arch_id && !en->isnt_a_tile) {
						int tile_radius = data.dist_from_self;
						if (v2_dist(spawn_pos, en->pos) < tile_width * tile_radius) {
							too_close = true;
							break;
						}
					}
				}

				if (!too_close) {
					Entity* en = entity_create(dim);
					entity_setup(en, data.arch_id);
					en->pos = spawn_pos;
				}

				iterations += 1;
				if (iterations > 300) {
					break;
				}
			}
		} else {
			log_warning("no potential tiles found for this dude");
		}
	}
}

// :map init
void world_setup() {
	Dimension dim = DIM_first;

	Entity* player_en = entity_create(dim);
	setup_player(player_en);
	player_en->pos.x = 20.f;

	Entity* en = entity_create(dim);
	setup_oxygenerator(en);
	world->oxygenerator = handle_from_entity(en);
	en->pos = snap_position_to_nearest_tile_based_on_arch(v2(0, 0), en->arch);

	en = entity_create(dim);
	setup_workbench(en);
	en->pos = snap_position_to_nearest_tile_based_on_arch(v2(-tile_width, tile_width), en->arch);

	en = entity_create(dim);
	setup_ice_vein(en);
	en->pos.x = 50;
	en->pos.y = 0;
	en->pos = snap_position_to_nearest_tile_based_on_arch(en->pos, en->arch);

	spawn_world_resources();

	world->item_unlocks[ARCH_research_station].research_progress = 100;
	world->item_unlocks[ARCH_workbench].research_progress = 100;

	// :test stuff
	#if defined(DEV_TESTING)
	{
		en = entity_create(dim);
		setup_portal(en);
		en->pos.x = 50;
		en->pos.y = 20;
		en->pos = snap_position_to_nearest_tile_based_on_arch(en->pos, en->arch);
		en->dimension_target = DIM_second;

		world->item_unlocks[ARCH_tether].research_progress = 100;

		world->inventory_items[0] = (ItemInstanceData){.id=ARCH_exp, .amount=9999};
		attempt_add_item_to_inv((ItemInstanceData){.id=ARCH_iron_ingot, .amount=300});
		attempt_add_item_to_inv((ItemInstanceData){.id=ARCH_o2_shard, .amount=100});
		attempt_add_item_to_inv((ItemInstanceData){.id=ARCH_rock, .amount=100});
		attempt_add_item_to_inv((ItemInstanceData){.id=ARCH_copper_ingot, .amount=100});
		attempt_add_item_to_inv((ItemInstanceData){.id=ARCH_fiber, .amount=100});
		attempt_add_item_to_inv((ItemInstanceData){.id=ARCH_meteorite_ingot, .amount=100});

		/*
		world->inventory_items[ARCH_raw_copper].amount = 50;
		world->inventory_items[ARCH_pine_wood].amount = 50;
		world->inventory_items[ARCH_coal].amount = 50;
		world->inventory_items[ARCH_o2_shard].amount = 50;
		world->inventory_items[ARCH_rock].amount = 1000;
		world->inventory_items[ARCH_exp].amount = 9999;
		world->inventory_items[ARCH_flint_axe].amount = 1;
		world->inventory_items[ARCH_flint].amount = 100;
		world->inventory_items[ARCH_fiber].amount = 100;
		world->inventory_items[ARCH_copper_ingot].amount = 100;
		*/

		// en = entity_create(dim);
		// setup_furnace(en);
		// en->pos.y = 20.0;

		// en = entity_create(dim);
		// setup_research_station(en);
		// en->pos.x = -20.0;
	}
	#endif
}

// :particle system
typedef enum ParticleFlags {
	PARTICLE_FLAGS_valid = (1<<0),
	PARTICLE_FLAGS_physics = (1<<1),
	PARTICLE_FLAGS_friction = (1<<2),
	PARTICLE_FLAGS_fade_out_with_velocity = (1<<3),
	PARTICLE_FLAGS_light = (1<<4),
} ParticleFlags;
typedef struct Particle {
	ParticleFlags flags;
	Vector4 col;
	Vector2 pos;
	Vector2 velocity;
	Vector2 acceleration;
	float friction;
	float lifetime_length;
	float64 lifetime_end_time;
	float fade_out_vel_range;
	float fade_in_pct;
	float fade_out_pct;
	Vector4 light_col;
	float light_intensity;
	float light_radius;
} Particle;
Particle particles[2048] = {0};
int particle_cursor = 0;

Particle* particle_new() {
	Particle* p = &particles[particle_cursor];
	particle_cursor += 1;
	if (particle_cursor >= ARRAY_COUNT(particles)) {
		particle_cursor = 0;
	}
	if (p->flags & PARTICLE_FLAGS_valid) {
		log_warning("too many particles, overwriting existing");
	}
	p->flags |= PARTICLE_FLAGS_valid;
	return p;
}
void particle_clear(Particle* p) {
	memset(p, 0, sizeof(Particle));
}
void particle_update() {
	for (int i = 0; i < ARRAY_COUNT(particles); i++) {
		Particle* p = &particles[i];
		if (!(p->flags & PARTICLE_FLAGS_valid)) {
			continue;
		}

		// set end time on first update frame of particle
		if (p->lifetime_length && p->lifetime_end_time == 0) {
			p->lifetime_end_time = now() + p->lifetime_length;
		}

		if (p->lifetime_end_time && has_reached_end_time(p->lifetime_end_time)) {
			particle_clear(p);
			continue;
		}

		if (p->flags & PARTICLE_FLAGS_fade_out_with_velocity
		&& v2_length(p->velocity) < 0.01) {
			particle_clear(p);
		}

		if (p->flags & PARTICLE_FLAGS_physics) {
			if (p->flags & PARTICLE_FLAGS_friction) {
				p->acceleration = v2_sub(p->acceleration, v2_mulf(p->velocity, p->friction));
			}
			p->velocity = v2_add(p->velocity, v2_mulf(p->acceleration, delta_t));
			Vector2 next_pos = v2_add(p->pos, v2_mulf(p->velocity, delta_t));
			p->acceleration = (Vector2){0};
			p->pos = next_pos;
		}
	}
}
void particle_render() {
	for (int i = 0; i < ARRAY_COUNT(particles); i++) {
		Particle* p = &particles[i];
		if (!(p->flags & PARTICLE_FLAGS_valid)) {
			continue;
		}

		Vector4 col = p->col;
		if (p->flags & PARTICLE_FLAGS_fade_out_with_velocity) {
			col.a *= float_alpha(fabsf(v2_length(p->velocity)), 0, p->fade_out_vel_range);
		}

		// fade in
		if (p->fade_in_pct && p->lifetime_length != 0) {
			float fade_length = p->lifetime_length * p->fade_in_pct;

			float64 particle_start_time = p->lifetime_end_time - p->lifetime_length;

			float alpha = float_alpha(now(), particle_start_time, particle_start_time + fade_length);
			col.a *= alpha;
		}

		// fade out
		if (p->fade_out_pct && p->lifetime_length != 0) {
			float fade_out_length = p->lifetime_length * p->fade_out_pct;

			// float64 particle_start_time = p->lifetime_end_time - p->lifetime_length;

			float64 start_fade_out_time = p->lifetime_end_time - fade_out_length;

			float alpha = 1.0-float_alpha(now(), start_fade_out_time, p->lifetime_end_time);
			col.a *= alpha;
		}

		Vector2 size = v2(1, 1);
		Vector2 draw_pos = v2_sub(p->pos, v2_mulf(size, 0.5));

		draw_rect_in_frame(draw_pos, size, col, current_draw_frame);

		if (p->flags & PARTICLE_FLAGS_light) {
			add_point_light(p->pos, p->light_col, p->light_radius, p->light_intensity * col.a);
		}
	}
}

typedef enum ParticleKind {
	PFX_footstep,
	PFX_hit,
	// :particle
} ParticleKind;
void particle_emit(Vector2 pos, ParticleKind kind) {
	switch (kind) {
		case PFX_footstep: {
			// ...
		} break;

		case PFX_hit: {
			for (int i = 0; i < 4; i++) {
				Particle* p = particle_new();
				p->flags |= PARTICLE_FLAGS_physics | PARTICLE_FLAGS_friction | PARTICLE_FLAGS_fade_out_with_velocity;
				p->pos = pos;
				p->velocity = v2_normalize(v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1)));
				p->velocity = v2_mulf(p->velocity, get_random_float32_in_range(200, 200));
				p->col = COLOR_WHITE;
				p->friction = 20.0f;
				p->fade_out_vel_range = 30.0f;
			}
		} break;
	}
}

// caveman :serialisation™️
bool world_save_to_disk() {
	return os_write_entire_file_s(STR("world"), (string){sizeof(World), (u8*)world});
}
bool world_attempt_load_from_disk() {
	string result = {0};
	bool succ = os_read_entire_file_s(STR("world"), &result, temp_allocator);
	if (!succ) {
		log_error("Failed to load world.");
		return false;
	}

	// NOTE, for errors I used to do stuff like this assert:
	// assert(result.count == sizeof(World), "world size has changed!");
	//
	// But since shipping to users, I've noticed that it's always better to gracefully fail somehow.
	// That's why this function returns a bool. We handle that at the callsite.
	// Maybe we want to just start up a new world, throw a user friendly error, or whatever as a fallback. Not just crash the game lol.

	if (result.count != sizeof(World)) {
		log_error("world size different to one on disk.");
		return false;
	}

	memcpy(world, result.data, result.count);

	// re-setup to override the static data
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* en = &world->entities[i];
		if (en->is_valid) {
			entity_setup(en, en->arch);
		}
	}

	return true;
}

void input_slot_new(Range2f rect, StorageSlot* slot) {

	if (range2f_contains(rect, get_mouse_pos_in_current_space())) {
		
		// interact with the slot
		{
			bool has_desired_item_in_hand = false;
			for (int i = 0; i < slot->desired_item_count; i++) {
				ArchetypeID desired_item = slot->desired_items[i];
				if (world->mouse_cursor_item.id == desired_item) {
					has_desired_item_in_hand = true;
					break;
				}
			}

			if (world->mouse_cursor_item.id) {
				if (slot->item.id) {
					if (slot->item.id == world->mouse_cursor_item.id) {
						// attempt stack
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							slot->item.amount += world->mouse_cursor_item.amount;
							world->mouse_cursor_item = (ItemInstanceData){0};
						}
					} else {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							if (has_desired_item_in_hand) {
								// swap
								ItemInstanceData temp = world->mouse_cursor_item;
								world->mouse_cursor_item = slot->item;
								slot->item = temp;
							} else {
								play_sound("event:/error");
							}
						}
					}
				} else {
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						if (has_desired_item_in_hand) {
							// place inside
							slot->item = world->mouse_cursor_item;
							world->mouse_cursor_item = (ItemInstanceData){0};
						} else {
							play_sound("event:/error");
						}
					}
				}
			} else {
				if (slot->item.id) {
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						// take into hand
						world->mouse_cursor_item = slot->item;
						slot->item = (ItemInstanceData){0};
					}

					item_tooltip(slot->item);
				}
			}
		}
	}

	draw_rect_in_frame(rect.min, range2f_size(rect), COLOR_GRAY, current_draw_frame);

	if (slot->item.id) {
		draw_item_amount_in_rect(slot->item, rect);
	}

}

void input_slot(Range2f rect, ItemInstanceData* slot, ArchetypeID* desired_items) {
	if (range2f_contains(rect, get_mouse_pos_in_current_space())) {
		
		// interact with the slot
		{

			bool has_desired_item_in_hand = false;
			for (int i = 0; i < growing_array_get_valid_count(desired_items); i++) {
				ArchetypeID desired_item = desired_items[i];
				if (world->mouse_cursor_item.id == desired_item) {
					has_desired_item_in_hand = true;
					break;
				}
			}

			if (world->mouse_cursor_item.id) {
				if (slot->id) {
					if (slot->id == world->mouse_cursor_item.id) {
						// attempt stack
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							slot->amount += world->mouse_cursor_item.amount;
							world->mouse_cursor_item = (ItemInstanceData){0};
						}
					} else {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							if (has_desired_item_in_hand) {
								// swap
								ItemInstanceData temp = world->mouse_cursor_item;
								world->mouse_cursor_item = *slot;
								*slot = temp;
							} else {
								play_sound("event:/error");
							}
						}
					}
				} else {
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						if (has_desired_item_in_hand) {
							// place inside
							*slot = world->mouse_cursor_item;
							world->mouse_cursor_item = (ItemInstanceData){0};
						} else {
							play_sound("event:/error");
						}
					}
				}
			} else {
				if (slot->id) {
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						// take into hand
						world->mouse_cursor_item = *slot;
						*slot = (ItemInstanceData){0};
					}

					item_tooltip(*slot);
				}
			}
		}
	}

	draw_rect_in_frame(rect.min, range2f_size(rect), COLOR_GRAY, current_draw_frame);

	if (slot->id) {
		draw_item_amount_in_rect(*slot, rect);
	}
}

// :ui :ux
void do_world_entity_interaction_ui_stuff() {

	// randy: I'm trying out making this UI be more diagetic and in the world.
	// that way we can just communicate the inputs/outputs and have the bare minimum info.
	// Instead of popping up a big ugly UI box.
	// I think it's roughly the same amount of work to do it this way, if proven effective.

	// clear the interacting with entity handle because we exit the UX state
	if (!is_nil(entity_from_handle(world->interacting_with_entity)) && world->ux_state == 0) {
		world->interacting_with_entity = (EntityHandle){0};
	}

	// exit out of this if we arent in the right UX state, or have no interactable entity to work on
	if (!(world->ux_state == UX_entity_interaction && !is_nil(entity_from_handle(world->interacting_with_entity)))) {
		return;
	}

	push_z_layer_in_frame(layer_ui, current_draw_frame);

	Entity* en = entity_from_handle(world->interacting_with_entity);

	float slot_size = 10.f;

	// :oxygenerator ui
	if (en->arch == ARCH_oxygenerator) {

		float x0 = en->pos.x;
		float y0 = en->pos.y;
		y0 += 6.f;

		Vector2 size = v2(10, 10);
		x0 -= size.x * 0.5;

		Range2f rect = range2f_make_bottom_left(v2(x0, y0), size);

		ArchetypeID* desired_items;
		growing_array_init_reserve((void**)&desired_items, sizeof(ArchetypeID), 1, get_temporary_allocator());
		ArchetypeID desired_item = ARCH_o2_shard;
		growing_array_add((void**)&desired_items, &desired_item);

		input_slot(rect, &en->input0, desired_items);

		if (!en->input0.id) {
			Draw_Quad* quad = draw_sprite_in_rect(get_sprite_id_from_item(ARCH_o2_shard), rect, COLOR_WHITE, 0.1);

			if (do_oxygenerator_error(en)) {
				set_col_override(quad, v4(sin_breathe(os_get_elapsed_seconds(), 6.f),0,0, 0.8));
			} else {
				set_col_override(quad, v4(0,0,0, 0.8));
			}
		}
	}

	// :burner ux
	if (en->arch == ARCH_burner_drill) {

		float slot_size = 10;

		float x0 = en->pos.x;
		float y0 = en->pos.y;

		Range2f render_range = get_entity_range(en);

		// fuel input & bar
		{
			x0 -= slot_size * 0.5;

			y0 = render_range.min.y - 1;

			y0 -= slot_size;

			Range2f rect = range2f_make_bottom_left(v2(x0, y0), v2(slot_size, slot_size));

			ArchetypeID* desired_items;
			growing_array_init_reserve((void**)&desired_items, sizeof(ArchetypeID), 1, get_temporary_allocator());
			ArchetypeID desired_item = ARCH_coal;
			growing_array_add((void**)&desired_items, &desired_item);

			input_slot(rect, &en->input0, desired_items);

			if (!en->input0.id) {
				Draw_Quad* quad = draw_sprite_in_rect(get_sprite_id_from_item(desired_item), rect, COLOR_WHITE, 0.1);

				if (en->last_frame.error == GAME_ERR_no_fuel) {
					set_col_override(quad, v4(sin_breathe(os_get_elapsed_seconds(), 6.f),0,0, 0.8));
				} else {
					set_col_override(quad, v4(0,0,0, 0.8));
				}
			}
			// fuel bar
			{
				x0 += slot_size + 1.f;

				Vector2 bar_size = v2(2, slot_size);

				draw_rect_in_frame(v2(x0, y0), bar_size, COLOR_BLACK, current_draw_frame);

				float alpha = float_alpha(en->current_fuel, 0, en->last_fuel_max);

				draw_rect_in_frame(v2(x0, y0), v2(bar_size.x, bar_size.y * alpha), col_fire, current_draw_frame);
			}
		}

		// drill output
		{
			x0 = en->pos.x;
			y0 = en->pos.y;

			x0 -= slot_size * 0.5;

			y0 = render_range.max.y + 1.f;

			// progress bar
			{
				Vector2 bar_size = v2(slot_size, 2);

				draw_rect_in_frame(v2(x0, y0), bar_size, COLOR_BLACK, current_draw_frame);

				float alpha = float_alpha(en->progress, 0, en->progress_max);

				draw_rect_in_frame(v2(x0, y0), v2(bar_size.x * alpha, bar_size.y), col_oxygen, current_draw_frame);

				y0 += bar_size.y + 1;
			}

			Range2f rect = range2f_make_bottom_left(v2(x0, y0), v2(slot_size, slot_size));

			// output slot
			{
				ItemInstanceData* slot = &en->output0;

				if (range2f_contains(rect, get_mouse_pos_in_current_space())) {
					// interact with the slot
					{
						if (world->mouse_cursor_item.id) {
							if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
								consume_key_just_pressed(MOUSE_BUTTON_LEFT);
								play_sound("event:/error");
							}
						} else {
							if (slot->id) {
								if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
									consume_key_just_pressed(MOUSE_BUTTON_LEFT);
									// take into hand
									world->mouse_cursor_item = *slot;
									*slot = (ItemInstanceData){0};
								}

								item_tooltip(*slot);
							}
						}
					}
				}

				Draw_Quad* q = draw_rect_in_frame(rect.min, range2f_size(rect), COLOR_GRAY, current_draw_frame);
				if (en->last_frame.error == GAME_ERR_no_room_in_destination) {
					set_col_override(q, get_red_error_col_override());
				}

				if (slot->id) {
					draw_item_amount_in_rect(*slot, rect);
				}
			}

		}
	}

	// :furnace ux
	if (en->arch == ARCH_furnace) {

		float x0 = en->pos.x;
		float y0 = en->pos.y;

		// fuel input
		Vector2 slot_size = v2(10, 10);
		x0 -= slot_size.x * 0.5;
		y0 += get_offset_for_rendering(en->arch).y;
		y0 -= slot_size.x + 1.f;
		{

			Range2f rect = range2f_make_bottom_left(v2(x0, y0), slot_size);

			StorageSlot* slot = &en->storage_slots[0];

			input_slot_new(rect, slot);

			if (!slot->item.id) {
				Draw_Quad* quad = draw_sprite_in_rect(SPRITE_coal, rect, COLOR_WHITE, 0.1);

				if (en->current_fuel == 0) {
					set_col_override(quad, v4(sin_breathe(os_get_elapsed_seconds(), 6.f),0,0, 0.8));
				} else {
					set_col_override(quad, v4(0,0,0, 0.8));
				}
			}

			// fuel bar
			{
				x0 += slot_size.x + 1.f;

				Vector2 bar_size = v2(2, slot_size.y);

				draw_rect_in_frame(v2(x0, y0), bar_size, COLOR_BLACK, current_draw_frame);

				float alpha = float_alpha(en->current_fuel, 0, en->last_fuel_max);

				draw_rect_in_frame(v2(x0, y0), v2(bar_size.x, bar_size.y * alpha), col_fire, current_draw_frame);
			}
		}

		// item input
		{
			x0 = en->pos.x;
			y0 = en->pos.y;

			x0 -= slot_size.x * 0.5;
			y0 = get_entity_range(en).max.y;
			y0 += 1.f;

			Range2f rect = range2f_make_bottom_left(v2(x0, y0), slot_size);

			StorageSlot* slot = &en->storage_slots[1];
			input_slot_new(rect, slot);

			// craft progress bar
			{
				y0 += slot_size.x + 1.f;

				Vector2 bar_size = v2(slot_size.y, 2);

				draw_rect_in_frame(v2(x0, y0), bar_size, COLOR_BLACK, current_draw_frame);

				float alpha = float_alpha(en->progress_on_crafting, 0, 100);

				draw_rect_in_frame(v2(x0, y0), v2(bar_size.x * alpha, bar_size.y), col_oxygen, current_draw_frame);
			}
		}

	}

	// :turret ux
	if (en->arch == ARCH_turret) {

		float x0 = en->pos.x;
		float y0 = en->pos.y;

		Vector2 slot_size = v2(10, 10);

		// item input
		{
			x0 = en->pos.x;
			y0 = en->pos.y;

			x0 -= slot_size.x * 0.5;
			y0 = get_entity_range(en).max.y;
			y0 += 1.f;

			ItemInstanceData* slot = &en->input0;

			ArchetypeID* desired_items;
			growing_array_init_reserve((void**)&desired_items, sizeof(ArchetypeID), 1, get_temporary_allocator());
			for (ArchetypeID i = 0; i < ARCH_MAX; i++) {
				Entity item_data = get_item_data(i);
				if (item_data.used_in_turret) {
					growing_array_add((void**)&desired_items, &i);
				}
			}

			Range2f rect = range2f_make_bottom_left(v2(x0, y0), slot_size);

			input_slot(rect, slot, desired_items);
			if (!slot->id) {
				Draw_Quad* quad = draw_sprite_in_rect(get_sprite_id_from_item(ARCH_bullet), rect, COLOR_WHITE, 0.1);

				set_col_override(quad, v4(sin_breathe(os_get_elapsed_seconds(), 6.f),0,0, 0.8));
			}
		}

	}

	// :storage ux
	if (en->arch == ARCH_wood_crate) {
		// slot_size.x;

		float x0 = en->pos.x;
		float y0 = en->pos.y;

		x0 -= slot_size * 0.5;
		y0 = get_entity_range(en).max.y + 1.f;

		{
			Range2f rect = range2f_make_bottom_left(v2(x0, y0), v2(slot_size, slot_size));

			ItemInstanceData* slot = &en->input0;

			if (range2f_contains(rect, get_mouse_pos_in_current_space())) {
				if (world->mouse_cursor_item.id) {
					if (slot->id) {
						if (slot->id == world->mouse_cursor_item.id) {
							// attempt stack
							if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
								consume_key_just_pressed(MOUSE_BUTTON_LEFT);
								slot->amount += world->mouse_cursor_item.amount;
								world->mouse_cursor_item = (ItemInstanceData){0};
							}
						} else {
							if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
								consume_key_just_pressed(MOUSE_BUTTON_LEFT);
								// swap
								ItemInstanceData temp = world->mouse_cursor_item;
								world->mouse_cursor_item = *slot;
								*slot = temp;
							}
						}
					} else {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							*slot = world->mouse_cursor_item;
							world->mouse_cursor_item = (ItemInstanceData){0};
						}
					}
				} else {
					if (slot->id) {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							// take into hand
							world->mouse_cursor_item = *slot;
							*slot = (ItemInstanceData){0};
						}

						item_tooltip(*slot);
					}
				}
			}

			draw_rect_in_frame(rect.min, range2f_size(rect), COLOR_GRAY, current_draw_frame);

			if (slot->id) {
				draw_item_amount_in_rect(*slot, rect);
			}
		}
	}

	// :thumper ux
	if (en->arch == ARCH_thumper) {
		ArchetypeID desired_item = ARCH_coal;

		float x0 = en->pos.x;
		float y0 = en->pos.y;
		y0 += 10.f;

		Vector2 size = v2(10, 10);
		x0 -= size.x * 0.5;

		bool do_tooltip = false;
		Range2f rect = range2f_make_bottom_left(v2(x0, y0), size);
		if (range2f_contains(rect, get_mouse_pos_in_current_space())) {
			
			// interact with the slot
			{
				if (world->mouse_cursor_item.id) {
					if (en->input0.id) {
						if (en->input0.id == world->mouse_cursor_item.id) {
							// attempt stack
							if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
								consume_key_just_pressed(MOUSE_BUTTON_LEFT);
								en->input0.amount += world->mouse_cursor_item.amount;
								world->mouse_cursor_item = (ItemInstanceData){0};
							}
						} else {
							if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
								consume_key_just_pressed(MOUSE_BUTTON_LEFT);
								if (world->mouse_cursor_item.id == desired_item) {
									// swap
									ItemInstanceData temp = world->mouse_cursor_item;
									world->mouse_cursor_item = en->input0;
									en->input0 = temp;
								} else {
									play_sound("event:/error");
								}
							}
						}
					} else {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							if (world->mouse_cursor_item.id == desired_item) {
								// place inside
								en->input0 = world->mouse_cursor_item;
								world->mouse_cursor_item = (ItemInstanceData){0};
							} else {
								play_sound("event:/error");
							}
						}
					}
				} else {
					if (en->input0.id) {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							// take into hand
							world->mouse_cursor_item = en->input0;
							en->input0 = (ItemInstanceData){0};
						}

						do_tooltip = true;
					}
				}
			}
		}

		draw_rect_in_frame(rect.min, size, COLOR_GRAY, current_draw_frame);

		if (en->input0.id) {
			draw_item_amount_in_rect(en->input0, rect);
		} else {
			Draw_Quad* quad = draw_sprite_in_rect(get_sprite_id_from_item(desired_item), rect, COLOR_WHITE, 0.1);

			if (en->current_fuel == 0) {
				set_col_override(quad, v4(sin_breathe(os_get_elapsed_seconds(), 6.f),0,0, 0.8));
			} else {
				set_col_override(quad, v4(0,0,0, 0.8));
			}
		}

		// fuel bar
		{
			x0 += size.x + 1.f;

			Vector2 bar_size = v2(2, size.y);

			draw_rect_in_frame(v2(x0, y0), bar_size, COLOR_BLACK, current_draw_frame);

			float alpha = float_alpha(en->current_fuel, 0, en->last_fuel_max);

			draw_rect_in_frame(v2(x0, y0), v2(bar_size.x, bar_size.y * alpha), col_fire, current_draw_frame);
		}

		if (do_tooltip) {
			item_tooltip(en->input0);
		}
	}

	// :qcontroller ux
	if (en->arch == ARCH_portal_controller) {
		// slot_size.x;

		float x0 = en->pos.x;
		float y0 = en->pos.y;

		x0 -= slot_size * 0.5;
		y0 = get_entity_range(en).max.y + 1.f;

		{
			Range2f rect = range2f_make_bottom_left(v2(x0, y0), v2(slot_size, slot_size));

			StorageSlot* slot = &en->storage_slots[0];

			input_slot_new(rect, slot);

			draw_rect_in_frame(rect.min, range2f_size(rect), COLOR_GRAY, current_draw_frame);

			if (slot->item.id) {
				draw_item_amount_in_rect(slot->item, rect);
			} else {
				Draw_Quad* quad = draw_sprite_in_rect(SPRITE_blank_tp_focus, rect, COLOR_WHITE, 0.1);
				set_col_override(quad, v4(0,0,0, 0.8));
			}
		}
	}

	pop_z_layer_in_frame(current_draw_frame);
}

// :ui
void do_ui_stuff() {
	set_screen_space();
	push_z_layer_in_frame(layer_ui, current_draw_frame);

	// "screen space"
	Vector2 text_scale = v2(0.1, 0.1);
	Vector4 bg_col = v4(0, 0, 0, 0.90);
	Vector4 fill_col = v4(0.5, 0.5, 0.5, 1.0);
	Vector4 accent_col = hex_to_rgba(0x44c3daff);

	Entity* interacting_with_entity = entity_from_handle(world->interacting_with_entity);

	// :exp amount
	/*
	{
		// animate the error flash
		if (exp_error_flash_alpha_target == 1.f) {
			bool reached = animate_f32_to_target_with_epsilon(&exp_error_flash_alpha, exp_error_flash_alpha_target, delta_t, 30.f, 0.01f);
			if (reached) {
				exp_error_flash_alpha_target = 0.f;
			}
		} else if (exp_error_flash_alpha_target == 0.f && exp_error_flash_alpha != 0.f) {
			animate_f32_to_target(&exp_error_flash_alpha, exp_error_flash_alpha_target, delta_t, 15.f);
		}

		Vector4 col = COLOR_WHITE;
		if (exp_error_flash_alpha) {
			col.xyz = v3_lerp(col.xyz, COLOR_RED.xyz, exp_error_flash_alpha);
		}
		string txt = tprint("%iml", get_player()->exp_amount);
		Vector2 pos = {0};
		pos.y += screen_height - 2.0f;
		pos.x += 1.0f;

		draw_text_with_pivot(font, txt, font_height_beeg, pos, text_scale, col, PIVOT_top_left);
	}
	*/

	// time til next :cycle
	{
		Vector4 col = COLOR_WHITE;

		string txt = tprint("Time until %s: %is", world->night_alpha_target == 0 ? "night" : "day", (int)(world->cycle_end_time - now()));
		Vector2 pos = {0};
		pos.y += screen_height - 2.0f;
		pos.x = screen_width;
		pos.x -= 1.0f;

		draw_text_with_pivot(font, txt, font_height, pos, text_scale, col, PIVOT_top_right);
	}

	// :respawn ui
	if (world->ux_state == UX_respawn) {
		u32 title_font_height = 128;
		u32 subtitle_font_height = 64;

		float x0 = screen_width * 0.5;
		float y0 = screen_height * 0.6666;
		draw_text_with_pivot(font, STR("DED"), title_font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_bottom_center);

		y0 -= 5.0;
		draw_text_with_pivot(font, STR("All Experience was lost."), subtitle_font_height, v2(x0, y0), text_scale, COLOR_RED, PIVOT_top_center);

		y0 -= 10.0;
		draw_text_with_pivot(font, STR("press 'R' to respawn"), subtitle_font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);

		if (is_key_just_pressed('R')) {
			consume_key_just_pressed('R');
			world->ux_state = UX_nil;
			Entity* player = get_player();
			player->health = 1;
			player->oxygen = player->oxygen_max;
			player->pos = v2(10, 0);
		}
	}

	// :inventory UI
	{
		if (is_key_just_pressed(KEY_TAB)) {
			consume_key_just_pressed(KEY_TAB);
			world->ux_state = (world->ux_state == UX_inventory ? UX_nil : UX_inventory);
		}
		world->inventory_alpha_target = (world->ux_state == UX_inventory ? 1.0 : 0.0);
		if (world_frame.show_inventory) {
			world->inventory_alpha_target = 1.f;
		}
		animate_f32_to_target(&world->inventory_alpha, world->inventory_alpha_target, delta_t, 15.0);
		bool is_inventory_enabled = world->inventory_alpha_target == 1.0;
		if (world->inventory_alpha_target != 0.0)
		{
			// TODO - some opacity thing here.

			float icon_width = 8.0;
			const int column_count = 8;
			const int row_count = 3;

			Vector2 bg_box_size = v2(column_count * icon_width, row_count * icon_width);

			float x0 = 0;
			float y0 = 0;
			x0 = screen_width * 0.5 - bg_box_size.x * 0.5;
			y0 = screen_height - 14.f;
			float y_top = y0;

			y0 -= bg_box_size.y;
			float x_left = x0;

			// bg box rendering thing
			{
				Range2f box = range2f_make_bottom_left(v2(x0, y0), bg_box_size);
				draw_rect_in_frame(box.min, bg_box_size, bg_col, current_draw_frame);

				// put :cursor item back
				if (world->mouse_cursor_item.id) {
					is_inventory_enabled = false;
					if (range2f_contains(box, get_mouse_pos_in_current_space())) {
						world_frame.hover_consumed = true;
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							bool succ = move_item_instance_to_inv(&world->mouse_cursor_item);
							if (!succ) {
								play_sound("event:/error");
							}
						}
					}
				}
			}

			x0 = x_left;
			y0 = y_top;
			y0 -= icon_width;

			int slot_index = 0;
			ItemInstanceData* hovered_item_slot = 0;
			for (int i = 0; i < ARRAY_COUNT(world->inventory_items); i++) {
				Entity item_data = get_item_data(i);
				ItemInstanceData* item = &world->inventory_items[i];
				if (item->amount > 0) {

					Range2f box = range2f_make_bottom_left(v2(x0, y0), v2(icon_width, icon_width));

					float is_selected_alpha = 0.0;

					draw_rect_in_frame(box.min, range2f_size(box), v4(1, 1, 1, 0.2), current_draw_frame);

					if (is_inventory_enabled && range2f_contains(box, get_mouse_pos_in_current_space())) {
						is_selected_alpha = 1.0;
						hovered_item_slot = item;
					}

					Sprite* sprite = get_sprite(get_sprite_id_from_item_instance(*item));

					draw_item_amount_in_rect(*item, box);

					// tooltip
					if (is_selected_alpha == 1.0)
					{
						item_tooltip(*item);
					}

					slot_index += 1;

					x0 += icon_width;
					if (slot_index % column_count == 0) {
						y0 -= icon_width;
						x0 = x_left;
					}
				}
			}

			// :cursor item
			if (!world->mouse_cursor_item.id && hovered_item_slot) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					world->mouse_cursor_item = *hovered_item_slot;
					*hovered_item_slot = empty_item;
				}
			}
		}
	}

	// :workbench :fabricator
	if (world->ux_state == UX_entity_interaction && interacting_with_entity->arch == ARCH_workbench) {
		Entity* entity = interacting_with_entity;

		Vector2 pane_size = v2(60.0, 70.0);
		float text_height_pad = 4.0;

		float x0 = screen_width * 0.5 - pane_size.x * 0.5;
		float y0 = screen_height * 0.5 - pane_size.y * 0.5;
		y0 -= 10;
		float x_left = x0;
		float x_middle = x0 + pane_size.x * 0.5;

		Range2f rect = range2f_make_bottom_left(v2(x0, y0), pane_size);
		if (range2f_contains(rect, get_mouse_pos_in_current_space())) {
			world_frame.hover_consumed = true;
		}

		draw_rect_in_frame(v2(x0, y0), pane_size, bg_col, current_draw_frame);

		y0 += pane_size.y;

		{
			y0 -= 2.f;
			x0 = x_middle;
			string txt = get_archetype_data(ARCH_workbench).pretty_name;
			Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
			y0 -= met.visual_size.y + 2.f;
		}

		x0 = x_left;

		float icon_length = 10.f;
		y0 -= icon_length;
		ArchetypeID selected = 0;
		int count = 0;
		for (ArchetypeID i = 1; i < ARCH_MAX; i++) {
			Entity item_data  = get_item_data(i);
			bool unlocked = is_fully_unlocked(world->item_unlocks[i]);
			if (item_data.ingredients_count == 0 || item_data.disabled) {
				continue;
			}

			count += 1;

			{
				Range2f box = range2f_make_bottom_left(v2(x0, y0), v2(icon_length, icon_length));

				Matrix4 xform = m4_identity;

				float scale = 1.f;
				if (unlocked && range2f_contains(box, get_mouse_pos_in_current_space())) {
					scale = 1.2f;
					selected = i;
				}

				xform = m4_translate(xform, v3(x0, y0, 1));
				xform = m4_translate(xform, v3(icon_length * 0.5, icon_length * 0.5, 1));
				xform = m4_scale(xform, v3(scale, scale, 1));
				xform = m4_translate(xform, v3(icon_length * -0.5, icon_length * -0.5, 1));

				Range2f render_box = range2f_make_bottom_left(v2(0, 0), v2(icon_length, icon_length));

				Draw_Quad* quad = draw_sprite_in_rect_with_xform(get_icon_from_arch_id(i), render_box, COLOR_WHITE, 0.2, xform);
				if (!unlocked) {
					set_col_override(quad, v4(0.2, 0.2, 0.2, 1.0));
				}
			}
			x0 += icon_length;

			// scuffed row advance
			if (count % 6 == 0) {
				y0 -= icon_length;
				x0 = x_left;
			}
		}

		if (selected)
		defer_scope(push_z_layer_in_frame(layer_tooltip, current_draw_frame), pop_z_layer_in_frame(current_draw_frame)) {
			UnlockState unlock_state = world->item_unlocks[selected];
			Entity item_data = get_item_data(selected);

			if (is_fully_unlocked(unlock_state)) {

				// craft
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);

					if (has_enough_for_recipe(item_data.ingredients, item_data.ingredients_count)) {
						// insta craft straight into cursor
						// #future, we'll wanna make this craft queue so we can lean into automated crafting

						if (!world->mouse_cursor_item.id
						|| (world->mouse_cursor_item.id == selected && world->mouse_cursor_item.amount + 1 <= item_data.stack_size)) {
							world->mouse_cursor_item.id = selected;
							world->mouse_cursor_item.amount += 1;
							consume_recipe(item_data.ingredients, item_data.ingredients_count);
							play_sound("event:/craft");
						} else {
							play_sound("event:/error");
						}
					} else {
						play_sound("event:/error");
					}

				}

				// tooltip
				{
					float width = 40.f;

					float x_left = get_mouse_pos_in_current_space().x;
					float y_top = get_mouse_pos_in_current_space().y;

					float x_middle = x_left + width * 0.5;

					float x0 = x_left;
					float y0 = y_top;

					float padding = 2.f;

					x0 = x_left + width * 0.5;
					y0 = y_top;
					y0 -= padding;

					// title
					{
						string txt = item_data.pretty_name;
						Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
						y0 -= met.visual_size.y + padding;
					}

					x0 = x_left + padding;

					{
						Gfx_Text_Metrics met = draw_text_with_pivot(font, STR("Ingredients:"), font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_left);
						y0 -= met.visual_size.y + padding;
					}

					x0 += 4.f;

					float x_left_ingredient_list = x0;

					// ingredient list
					// #duplicate
					for (int i = 0; i < item_data.ingredients_count; i++) {
						ItemInstanceData ing = item_data.ingredients[i];
						Entity ing_data = get_item_data(ing.id);

						float height = font_height_body * text_scale.x;

						x0 = x_left_ingredient_list;

						Vector4 col = COLOR_WHITE;
						if (!has_enough_for_recipe(&ing, 1)) {
							col = COLOR_RED;
						}

						{
							Range2f rect = range2f_make_top_left(v2(x0, y0), v2(height, height));
							draw_sprite_in_rect(ing_data.icon, rect, COLOR_WHITE, 0);
							x0 += height + 1.f;
						}

						string txt = tprint("%ix %s", ing.amount, ing_data.pretty_name);
						Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height_body, v2(x0, y0), text_scale, col, PIVOT_top_left);
						y0 -= met.visual_size.y + padding;
					}

					y0 -= padding;

					// description
					{
						x0 = x_middle;

						float wrap_width = width;
						string text = item_data.description;

						string* lines = split_text_to_lines_with_wrapping(text, wrap_width, font, font_height_body, text_scale, true);
						for (int i = 0; i < growing_array_get_valid_count(lines); i++) {
							string line = lines[i];
							Gfx_Text_Metrics metrics = draw_text_with_pivot(font, line, font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
							y0 -= metrics.visual_size.y;
						}
					}

					y0 -= padding;

					float height = y0 - y_top;

					Draw_Quad* quad = draw_rect_in_frame(v2(x_left, y_top), v2(width, height), v4(0.2, 0.2, 0.2, 1.), current_draw_frame);
					quad->z = layer_tooltip-1;
				}
			}
		}
	}

	// :research ui
	if (world->ux_state == UX_entity_interaction && interacting_with_entity->arch == ARCH_research_station) {
		Entity* entity = interacting_with_entity;

		Vector2 pane_size = v2(60.0, 70.0);
		float text_height_pad = 4.0;

		float x0 = screen_width * 0.5 - pane_size.x * 0.5;
		float y0 = screen_height * 0.5 - pane_size.y * 0.5;
		y0 -= 10;
		float x_left = x0;
		float x_middle = x0 + pane_size.x * 0.5;

		draw_rect_in_frame(v2(x0, y0), pane_size, bg_col, current_draw_frame);

		y0 += pane_size.y;

		// title
		{
			y0 -= 2.f;
			x0 = x_middle;
			string txt = get_archetype_data(entity->arch).pretty_name;
			Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
			y0 -= met.visual_size.y + 2.f;
			x0 = x_left;
		}

		float icon_length = 10.f;
		y0 -= icon_length;
		ArchetypeID selected = 0;
		int count = 0;
		for (int i = 1; i < ARCH_MAX; i++) {
			UnlockState unlock_state = world->item_unlocks[i];
			Entity item_data = get_item_data(i);
			if (item_data.ingredients_count == 0 || item_data.disabled || is_fully_unlocked(unlock_state)) {
				continue;
			}

			count += 1;

			{
				Range2f box = range2f_make_bottom_left(v2(x0, y0), v2(icon_length, icon_length));

				Matrix4 xform = m4_identity;

				float scale = 1.f;
				if (range2f_contains(box, get_mouse_pos_in_current_space())) {
					scale = 1.2f;
					selected = i;
				}

				Range2f render_box = range2f_make_bottom_left(v2(0, 0), v2(icon_length, icon_length));
				xform = m4_translate(xform, v3(x0, y0, 1));
				xform = m4_translate(xform, v3(icon_length * 0.5, icon_length * 0.5, 1));
				xform = m4_scale(xform, v3(scale, scale, 1));
				xform = m4_translate(xform, v3(icon_length * -0.5, icon_length * -0.5, 1));

				draw_sprite_in_rect_with_xform(get_icon_from_arch_id(i), render_box, COLOR_WHITE, 0.2, xform);
			}
			x0 += icon_length;

			// scuffed row advance
			if (count % 6 == 0) {
				y0 -= icon_length;
				x0 = x_left;
			}
		}

		if (selected) {
			UnlockState* unlock_state = &world->item_unlocks[selected];
			Entity item_data = get_item_data(selected);

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				bool has_ingredients = has_enough_for_recipe(item_data.research_ingredients, item_data.research_ingredients_count);

				if (has_ingredients) {
					consume_recipe(item_data.research_ingredients, item_data.research_ingredients_count);
					unlock_state->research_progress = 100;
					play_sound("event:/research");
				} else {
					play_sound("event:/error");
					exp_error_flash_alpha_target = 1.0f;
				}
			}

			Vector2 size = v2(40, 30);

			float x_left = get_mouse_pos_in_current_space().x;
			float y_top = get_mouse_pos_in_current_space().y;
			float x_middle = x_left + size.x * 0.5;
			float y_bottom = y_top - size.y;

			float x0 = x_left;
			float y0 = y_top;

			y0 = y_bottom;
			draw_rect_in_frame(v2(x0, y0), size, v4(0.2, 0.2, 0.2, 1.), current_draw_frame);

			x0 = x_left + size.x * 0.5;
			y0 = y_top;
			y0 -= 2.f;

			// title
			{
				string txt = item_data.pretty_name;
				Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
				y0 -= met.visual_size.y + 2.f;
			}

			// description
			{
				x0 = x_middle;

				float wrap_width = size.x;
				string text = item_data.description;

				string* lines = split_text_to_lines_with_wrapping(text, wrap_width, font, font_height_body, text_scale, true);
				for (int i = 0; i < growing_array_get_valid_count(lines); i++) {
					string line = lines[i];
					Gfx_Text_Metrics metrics = draw_text_with_pivot(font, line, font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
					y0 -= metrics.visual_size.y;
				}
			}

			y0 = y_bottom;
			y0 += 4.f;

			// ingredient list
			// #duplicate
			float x_left_ingredient_list = x_left;
			for (int i = 0; i < item_data.research_ingredients_count; i++) {
				ItemInstanceData ing = item_data.research_ingredients[i];
				Entity ing_data = get_item_data(ing.id);

				float height = font_height_body * text_scale.x;

				x0 = x_left_ingredient_list;

				Vector4 col = COLOR_WHITE;
				if (!has_enough_for_recipe(&ing, 1)) {
					col = COLOR_RED;
				}

				{
					Range2f rect = range2f_make_top_left(v2(x0, y0), v2(height, height));
					draw_sprite_in_rect(ing_data.icon, rect, COLOR_WHITE, 0);
					x0 += height + 1.f;
				}

				string txt = tprint("%ix %s", ing.amount, ing_data.pretty_name);
				Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height_body, v2(x0, y0), text_scale, col, PIVOT_top_left);
				y0 -= met.visual_size.y + 2.f;
			}
		}
	}

	// cursor 'Q' quit
	if (world->mouse_cursor_item.id) {
		if (is_key_just_pressed('Q')) {
			consume_key_just_pressed('Q');
			bool succ = move_item_instance_to_inv(&world->mouse_cursor_item);
			if (!succ) {
				play_sound("event:/error");
				// drop remaining on ground
				drop_item_at_pos(world->mouse_cursor_item, get_player()->pos, get_player_dim());
				world->mouse_cursor_item = empty_item;
			}
		}
	}

	set_world_space();
	pop_z_layer_in_frame(current_draw_frame);
}

// :cursor item drawing
void do_cursor_drawing_and_logic() {
	Dimension dim = get_player_dim();

	if (world->mouse_cursor_item.id)
	defer_scope(push_z_layer_in_frame(layer_cursor_item, current_draw_frame), pop_z_layer_in_frame(current_draw_frame))
	{
		ArchetypeID item_id = world->mouse_cursor_item.id;
		Entity item_data = get_item_data(world->mouse_cursor_item.id);
		if (!item_data.can_be_placed || world_frame.hover_consumed) {
			// it's just an item
			Sprite* sprite = get_sprite(get_sprite_id_from_item_instance(world->mouse_cursor_item));
			Range2f rect = range2f_make_center_center(get_mouse_pos_in_current_space(), v2(10, 10));
			draw_item_amount_in_rect(world->mouse_cursor_item, rect);

			if (item_id == ARCH_tp_focus) {
				world_frame.hover_consumed=true;
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					play_sound("event:/exp_pickup");
					world->mouse_cursor_item.focus_target_pos = get_mouse_pos_in_current_space();
					world->mouse_cursor_item.has_focus_target = true;
				}
			}

		} else
			defer_scope(set_world_space(), set_screen_space())
			defer_scope(push_z_layer_in_frame(layer_buildings, current_draw_frame), pop_z_layer_in_frame(current_draw_frame))
		{
			// :place building
			world_frame.hover_consumed = true;

			SpriteID sprite_id = item_data.sprite_id;
			ArchetypeID arch_id = item_data.arch;
			Entity arch_data = get_archetype_data(arch_id);

			if (is_key_just_pressed('R')) {
				consume_key_just_pressed('R');
				world->cursor_rotate_dir = world->cursor_rotate_dir + 1;
				if (world->cursor_rotate_dir > 3) {
					world->cursor_rotate_dir = 0;
				}
			}

			// manual override for conveyor
			if (arch_id == ARCH_conveyor) {
				if (world->cursor_rotate_dir == DIR_east) {
					sprite_id = SPRITE_conveyor_right;
				} else if (world->cursor_rotate_dir == DIR_south) {
					sprite_id = SPRITE_conveyor_down;
				} else if (world->cursor_rotate_dir == DIR_west) {
					sprite_id = SPRITE_conveyor_left;
				} else if (world->cursor_rotate_dir == DIR_north) {
					sprite_id = SPRITE_conveyor_up;
				} else {
					sprite_id = 0;
				}
			} else if (arch_id == ARCH_extractor) {
				if (world->cursor_rotate_dir == DIR_east) {
					sprite_id = SPRITE_extractor_east;
				} else if (world->cursor_rotate_dir == DIR_south) {
					sprite_id = SPRITE_extractor_south;
				} else if (world->cursor_rotate_dir == DIR_west) {
					sprite_id = SPRITE_extractor_west;
				} else if (world->cursor_rotate_dir == DIR_north) {
					sprite_id = SPRITE_extractor_north;
				} else {
					sprite_id = 0;
				}
			}

			Sprite* icon = get_sprite(sprite_id);

			Vector2 pos = get_mouse_pos_in_current_space();
			pos = snap_position_to_nearest_tile_based_on_arch(pos, arch_id);

			bool has_room_to_place = true;
			Tile* tiles = get_tile_list_at_pos_based_on_arch(pos, arch_id);
			for (int i = 0; i < growing_array_get_valid_count(tiles); i++) {
				Tile tile = tiles[i];
				TileCache* tc = tile_cache_at_tile(tile, dim);
				// tc->debug_col_override = COLOR_GREEN;
				// tc->debug_col_override.a = 0.2;

				if (tc->entity) {
					has_room_to_place = false;
				}
			}

			// range preview
			if (arch_data.radius)
			{
				float radius = arch_data.radius;
				// #polish - make this an outline?
				draw_circle_in_frame(v2_sub(pos, v2(radius, radius)), v2(radius*2, radius*2), v4(1, 1, 1, 0.05), current_draw_frame);
			}

			// draw preview
			{
				Matrix4 xform = m4_identity;
				Vector2 sprite_size = get_sprite_size(icon);
				// #volatile with entity rendering
				Vector2 draw_pos = v2_add(pos, get_offset_for_rendering(arch_id));
				xform = m4_translate(xform, v3(draw_pos.x, draw_pos.y, 0));

				Vector4 col_override = {0};
				if (!has_room_to_place) {
					col_override = COLOR_RED;
					col_override.a = 0.4;
				}

				Draw_Quad* quad = draw_image_xform_in_frame(icon->image, xform, get_sprite_size(icon), COLOR_WHITE, current_draw_frame);
				set_col_override(quad, col_override);
			}

			// :tether connection preview
			if (arch_data.is_oxygen_tether)
			{
				for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
					Entity* tether = &world->entities[i];
					if (tether->is_valid && tether->is_oxygen_tether && tether->last_frame.is_powered) {
						if (v2_dist(tether->pos, pos) < tether_connection_radius) {
							draw_line(v2_add(tether->pos, tether->tether_connection_offset), v2_add(pos, arch_data.tether_connection_offset), 1.0f, col_tether);
							break;
						}
					}
				}
			}

			// draw remaining
			if (world->mouse_cursor_item.amount > 1) {
				Vector2 text_scale = v2(0.1, 0.1);
				string txt = tprint("%i", world->mouse_cursor_item.amount);
				Vector2 txt_pos = pos;
				draw_text_with_pivot(font, txt, font_height, txt_pos, text_scale, COLOR_BLACK, PIVOT_center_center);
			}

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if (has_room_to_place) {
					Entity* en = entity_create(get_player_dim());
					entity_setup(en, arch_id);
					en->pos = pos;
					en->right_click_remove = true;
					en->dir = world->cursor_rotate_dir;

					world->mouse_cursor_item.amount -= 1;
					if (world->mouse_cursor_item.amount <= 0) {
						world->mouse_cursor_item = (ItemInstanceData){0};
					}
				} else {
					play_sound("event:/error");
				}
			}
		}
	}
}

Draw_Quad* draw_sprite_at_pos_pivot(SpriteID sprite_id, Vector2 pos, Pivot pivot) {
	Sprite* sprite = get_sprite(sprite_id);
	Vector2 draw_pos = pos;
	draw_pos = v2_sub(draw_pos, v2_mul(get_sprite_size(sprite), get_pivot_scale(pivot)));
	return draw_image_in_frame(sprite->image, draw_pos, v2(sprite->image->width, sprite->image->height), COLOR_WHITE, current_draw_frame);
}
Draw_Quad* draw_sprite(SpriteID sprite_id, Vector2 pos) {
	return draw_sprite_at_pos_pivot(sprite_id, pos, PIVOT_bottom_center);
}

void draw_base_sprite(Entity* en) {
	SpriteID sprite_id = en->sprite_id;
	if (!sprite_id) {
		sprite_id = en->icon;
	}

	Sprite* sprite = get_sprite(sprite_id);
	Vector2 size = get_sprite_size(sprite);
	Matrix4 xform = m4_scalar(1.0);

	// all entities basically have a center center position
	// that way the ->pos is very easily used in other areas intuitively.
	Vector2 draw_pos = v2_add(en->pos, get_offset_for_rendering(en->arch));
	xform = m4_translate(xform, v3(draw_pos.x, draw_pos.y, 0));

	// flip the sprite
	{
		xform = m4_translate(xform, v3(size.x * 0.5, size.y * 0.5, 0));
		xform = m4_scale(xform, v3(en->flip_sprite ? -1 : 1, 1, 1));
		xform = m4_translate(xform, v3(size.x * -0.5, size.y * -0.5, 0));
	}

	Vector4 col = COLOR_WHITE;
	if (world_frame.selected_entity == en) {
		if (en->frame.can_interact) {
			col = col_select;
		} else {
			col = v4(0.5, 0.5, 0.5, 1);
		}
	}

	if (en->wall_seal && en->frame.is_powered) {
		col = v4_mul(col, col_oxygen);
	}

	if (en->white_flash_current_alpha != 0) {
		animate_f32_to_target(&en->white_flash_current_alpha, 0, delta_t, 20.0f);
	}

	Draw_Quad* q = draw_image_xform_in_frame(sprite->image, xform, get_sprite_size(sprite), col, current_draw_frame);
	set_col_override(q, v4(1, 1, 1, en->white_flash_current_alpha));

	// :error flash
	bool do_error_flash = en->last_frame.error;
	// :burner drill
	if (en->arch == ARCH_burner_drill && en->current_fuel == 0) {
		do_error_flash = true;
	}
	// :oxygenerator
	if (en->arch == ARCH_oxygenerator && do_oxygenerator_error(en)) {
		do_error_flash = true;
	}

	if (do_error_flash) {
		set_col_override(q, v4(1, 0, 0, 0.8 * sin_breathe(os_get_elapsed_seconds(), 6.f)));
	}

	// debug pos 
	// draw_text(font, sprint(temp, STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);
}

// update :func dump

void render_player(Entity* en) {
	if (!is_player_alive()) {
		return; 
	}

	en->frame.functional_sprite_id = SPRITE_player_idle;

	if (en->frame.input_axis.x != 0 || en->frame.input_axis.y != 0) {
		en->frame.functional_sprite_id = SPRITE_player_walk;
	}

	if (en->frame.functional_sprite_id != en->last_frame.functional_sprite_id) {
		en->anim_index = 0;
		en->time_til_next_frame = 0;
	}

	Sprite* sprite = get_sprite(en->frame.functional_sprite_id);

	if (has_reached_end_time(en->time_til_next_frame)) {
		en->anim_index += 1;
		if (en->anim_index >= sprite->frames) {
			en->anim_index = 0;
		}
		en->time_til_next_frame = now() + 0.1;
	}

	Vector2i size = v2i(sprite->image->width / sprite->frames, sprite->image->height);

	Matrix4 xform = m4_scalar(1.0);
	xform         = m4_translate(xform, v3(0, tile_width * -0.5, 0));
	xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
	if (en->last_move_dir.x == -1) {
		xform = m4_scale(xform, v3(-1, 1, 1)); // flip if moving left
	}
	xform         = m4_translate(xform, v3(size.x * -0.5, 0.0, 0));

	Vector4 col = COLOR_WHITE;
	Draw_Quad* quad = draw_image_xform_in_frame(sprite->image, xform, v2(size.x, size.y), col, current_draw_frame);

	u32 anim_sheet_pos_x = en->anim_index * size.x;
	u32 anim_sheet_pos_y = 0;
	quad->uv.x1 = (float32)(anim_sheet_pos_x)/(float32)sprite->image->width;
	quad->uv.y1 = (float32)(anim_sheet_pos_y)/(float32)sprite->image->height;
	quad->uv.x2 = (float32)(anim_sheet_pos_x+size.x) /(float32)sprite->image->width;
	quad->uv.y2 = (float32)(anim_sheet_pos_y+size.y)/(float32)sprite->image->height;
}

// :thumper
void update_thumper(Entity* en) {

	// fuel processing
	if (en->current_fuel <= 0) {
		// attempt fuel consume
		if (en->input0.id) {
			en->last_fuel_max = 30;
			en->current_fuel = 30;

			en->input0.amount -= 1;
			if (en->input0.amount <= 0) {
				en->input0 = (ItemInstanceData){0};
			}
		}
	}

	if (en->current_fuel == 0) {
		en->frame.error = GAME_ERR_no_fuel;
	}

	// hit timer processing
	if (en->current_fuel > 0) {
		if (en->next_hit_end_time == 0) {
			en->next_hit_end_time = now() + 1.f;
		}
		if (has_reached_end_time(en->next_hit_end_time)) {
			en->next_hit_end_time = 0;

			// get all entities in radius
			bool did_hit_something = false;
			for (int j = 0; j < MAX_ENTITY_COUNT; j++) {
				Entity* against = &world->entities[j];
				if (against->destroyable_world_item && v2_dist(en->pos, against->pos) < en->radius) {
					did_hit_something = true;

					against->white_flash_current_alpha = 1.0;
					particle_emit(against->pos, PFX_hit);
					against->health -= 1;

					play_sound_at_pos("event:/hit_generic", against->pos);

					if (against->health <= 0) {
						do_entity_drops(against);
						do_entity_exp_drops(against);
						entity_zero_immediately(against);
					}
				}
			}

			if (did_hit_something) {
				camera_shake_at_pos(0.1, en->pos, 50, 50);
				en->current_fuel -= 1;
			}
		}
	}

}

// :portal
void do_portal_teleport_thing (Entity* player) {
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* portal = &world->entities[i];
		if (!(portal->is_valid && portal->arch == ARCH_portal && portal->dim == get_player_dim())) continue;

		float portal_width = tile_width * 7;

		Vector2 portal_thres_pos = portal->pos;
		portal_thres_pos.y += get_offset_for_rendering(portal->arch).y;

		float y_thres = portal_thres_pos.y;
		float x_min = portal->pos.x - portal_width * 0.5;
		float x_max = portal->pos.x + portal_width * 0.5;

		if (player->pos.x > x_min && player->pos.x < x_max
		&& player->frame.last_pos.y < y_thres && player->pos.y >= y_thres) {
			Vector2 old_player_pos = player->pos;
			
			Vector2 relative_to_portal = v2_sub(player->pos, portal->pos);
			player->pos = v2_add(portal->portal_view_pos, relative_to_portal);
			player->dim = portal->dimension_target;

			Vector2 relative_cam_pos = v2_sub(camera_pos, old_player_pos);
			camera_pos = v2_add(player->pos, relative_cam_pos);

			set_world_view();

			play_sound_at_pos("event:/teleport", player->pos);
			camera_shake(0.2);

			portal->teleported_at_time = now();
		}
	}
}
// :portal
void update_portal(Entity* en) {
	// if (is_key_down(MOUSE_BUTTON_RIGHT)) {
	// 	en->portal_view_pos = get_mouse_pos_in_current_space();
	// }

	if (!en->render_target_image) {
		Vector2 sprite_size = get_sprite_size(get_sprite(SPRITE_portal_frame));
		float res = 10.f;
		en->render_target_image = make_image_render_target(sprite_size.x * res, sprite_size.y * res, 4, 0, get_heap_allocator());
	}

}
// :portal
void render_portal(Entity* en) {

	// Vector2 size = get_sprite_size(get_sprite(SPRITE_portal_frame));
	// Vector2 draw_pos = en->pos;
	// draw_pos.x -= size.x * 0.5;
	// draw_image_in_frame(en->render_target_image, draw_pos, size, v4(1,1,1,1), current_draw_frame);

	Vector2 draw_pos = en->pos;
	draw_pos = v2_add(draw_pos, get_offset_for_rendering(en->arch));

	Draw_Quad* q = draw_sprite_at_pos_pivot(SPRITE_portal_frame, draw_pos, PIVOT_bottom_left);
	set_quad_type(q, QUAD_TYPE_portal);

	// afterimage effect
	// me don't like this...
	if (false)
	{
		float tp_length = 0.3f;
		float time_ago = now() - en->teleported_at_time;

		if (en->teleported_at_time && time_ago > tp_length) {
			en->teleported_at_time = 0;
		}

		if (en->teleported_at_time) {
			float alpha = alpha_from_end_time(en->teleported_at_time + tp_length, tp_length);

			q = draw_sprite(SPRITE_portal_frame, en->portal_view_pos);
			set_quad_type(q, QUAD_TYPE_portal);

			q->color.a = ease_in_exp(1.0-alpha, 15);
		}
	}
}

// :qcontroller
void update_portal_controller(Entity* en) {
	StorageSlot* slot = &en->storage_slots[0];
	if (slot->item.id && slot->item.has_focus_target) {

		// check for portals around us to update
		for (int i = 0; i < 4; i++) {
			Tile t = v2_world_pos_to_tile_pos(en->pos);
			if (i == 0) {
				t.x += 1;
			} else if (i == 1) {
				t.x -= 1;
			} else if (i == 2) {
				t.y += 1;
			} else if (i == 3) {
				t.y -= 1;
			}

			TileCache* tc = tile_cache_at_tile(t, en->dim);
			if (tc->entity && tc->entity->arch == ARCH_portal) {
				tc->entity->frame.focus_item = &slot->item;
			}
		}

	}
}

// :conveyor
void conveyor_move_logic(Entity* en) {

	Tile output_tile = v2_world_pos_to_tile_pos(en->pos);
	if (en->dir == DIR_east) {
		output_tile.x += 1;
	} else if (en->dir == DIR_south) {
		output_tile.y -= 1;
	} else if (en->dir == DIR_west) {
		output_tile.x -= 1;
	} else if (en->dir == DIR_north) {
		output_tile.y += 1;
	}
	TileCache* tc = tile_cache_at_tile(output_tile, en->dim);

	// attempt move into next tile
	if (tc->entity && en->input0.id) {
		Entity* storage = tc->entity;

		if (storage->arch == ARCH_conveyor) {
			if (storage->input0.id == 0) {
				if (en->next_update_end_time == 0) {
					en->next_update_end_time = now() + 0.5;
				}
				if (has_reached_end_time(en->next_update_end_time)) {
					en->next_update_end_time = 0;

					storage->input0.id = en->input0.id;
					storage->input0.amount = 1;
					en->input0.amount -= 1;
					if (en->input0.amount != 0) {
						log_error("this should be 0??");
					}
					en->input0 = (ItemInstanceData){0};
				}
			}
		} else {

			ItemInstanceData* item_on_conveyor = &en->input0;

			// loop thru slots, checking for desired matches
			for (int i = 0; i < storage->storage_slot_count; i++) {
				StorageSlot* dest_slot = &storage->storage_slots[i];
				if (dest_slot->output_only) {
					continue;
				}

				bool is_desired_item;
				if (dest_slot->desired_item_count) {
					for (int j = 0; j < dest_slot->desired_item_count; j++) {
						if (dest_slot->desired_items[j] == item_on_conveyor->id) {
							is_desired_item = true;
							break;
						}
					}
				} else {
					is_desired_item = true;
				}

				if (is_desired_item && (dest_slot->item.id == 0 || dest_slot->item.id == item_on_conveyor->id)) {

					if (en->next_update_end_time == 0) {
						en->next_update_end_time = now() + 0.5;
					}
					if (has_reached_end_time(en->next_update_end_time)) {
						en->next_update_end_time = 0;

						dest_slot->item.id = item_on_conveyor->id;
						dest_slot->item.amount += 1;
						*item_on_conveyor = (ItemInstanceData){0};
					}

				}
			}
		}
	}
}

void update_extractor(Entity* en) {

	// set proper sprite
	if (en->dir == DIR_east) {
		en->sprite_id = SPRITE_extractor_east;
	} else if (en->dir == DIR_south) {
		en->sprite_id = SPRITE_extractor_south;
	} else if (en->dir == DIR_west) {
		en->sprite_id = SPRITE_extractor_west;
	} else if (en->dir == DIR_north) {
		en->sprite_id = SPRITE_extractor_north;
	}

	conveyor_move_logic(en);

	if (!en->input0.id) {
		Tile input_tile = v2_world_pos_to_tile_pos(en->pos);
		if (en->dir == DIR_east) {
			input_tile.x -= 1;
		} else if (en->dir == DIR_south) {
			input_tile.y += 1;
		} else if (en->dir == DIR_west) {
			input_tile.x += 1;
		} else if (en->dir == DIR_north) {
			input_tile.y -= 1;
		}
		TileCache* tc = tile_cache_at_tile(input_tile, en->dim);

		if (tc->entity) {
			Entity* storage = tc->entity;

			// #improvement - need to define a "extractor takes slot from this slot" markup in setup ?
			ItemInstanceData* extraction_slot = &storage->output0;
			if (storage->arch == ARCH_wood_crate || storage->arch == ARCH_conveyor || storage->arch == ARCH_extractor) {
				extraction_slot = &storage->input0;
			}

			// pull from direction
			if (extraction_slot->id) {
				en->input0.id = extraction_slot->id;
				en->input0.amount = 1;

				extraction_slot->amount -= 1;
				if (extraction_slot->amount <= 0) {
					*extraction_slot = (ItemInstanceData){0};
				}
			}
		}
	}
}

void update_conveyor(Entity* en) {
	
	// set proper sprite
	if (en->dir == DIR_east) {
		en->sprite_id = SPRITE_conveyor_right;
	} else if (en->dir == DIR_south) {
		en->sprite_id = SPRITE_conveyor_down;
	} else if (en->dir == DIR_west) {
		en->sprite_id = SPRITE_conveyor_left;
	} else if (en->dir == DIR_north) {
		en->sprite_id = SPRITE_conveyor_up;
	}

	conveyor_move_logic(en);
}

void render_conveyor(Entity* en) {
	draw_base_sprite(en);

	if (en->input0.id) {
		draw_sprite(get_item_data(en->input0.id).icon, en->pos);
	}
}

// :nest
void update_enemy_nest(Entity* en) {

	int enemy_count = 0;
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* against = &world->entities[i];
		if (is_valid(against) && against->arch == ARCH_enemy1 && against->spawned_from.id == en->id) {
			enemy_count += 1;
		}
	}

	if (en->last_frame.is_creation) {
		en->next_hit_end_time = now();
	}

	if (enemy_count < 1 && en->next_hit_end_time == 0.0) {
		en->next_hit_end_time = now() + get_random_float32_in_range(20, 40);
	}

	if (enemy_count < 1 && en->next_hit_end_time != 0 && has_reached_end_time(en->next_hit_end_time)) {
		Entity* spawn = entity_create(en->dim);
		setup_enemy1(spawn);
		spawn->pos = en->pos;
		spawn->spawned_from = handle_from_entity(en);

		en->next_hit_end_time = 0;
	}

}

// :nest
void render_enemy_nest(Entity* en) {
	draw_base_sprite(en);

	add_point_light(en->pos, COLOR_RED, 20, 0.5);
}

// :meteor
float meteor_strike_length = 3.5f;
void update_meteor(Entity* en) {

	bool is_distant_meteor = v2_dist(get_player()->pos, en->pos) > 250.f;

	if (en->next_hit_end_time == 0) {
		en->next_hit_end_time = now() + meteor_strike_length;

		if (is_distant_meteor) {
			play_sound_at_pos("event:/meteor_crash_far", en->pos);
		} else {
			play_sound_at_pos("event:/meteor_crash", en->pos);
		}
	}

	if (has_reached_end_time(en->next_hit_end_time)) {

		if (is_distant_meteor) {
			camera_shake(0.15);
		} else {
			// somewhat #volatile with fmod sound spatialisation range
			float start_falloff = 80.f;
			float end_falloff = 500.f;
			camera_shake_at_pos(0.5, en->pos, start_falloff, end_falloff-start_falloff);
		}

		// damage entities in a radius
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* against = &world->entities[i];
			if (is_valid(against) && against->destroyable_by_explosion) {
				float dist = v2_dist(against->pos, en->pos);
				if (dist < en->radius) {
					if (against->arch == ARCH_player) {
						against->oxygen = 0;
					} else {
						if (!against->meteor_destroy_without_drops) {
							do_entity_drops(against);
						}
						entity_zero_immediately(against);
					}
				}
			}
		}

		// spawn in new resources
		int meteorite_count = 0;
		for (int i = 0; i < get_random_int_in_range(3, 5); i++) {
			
			// :meteor drops 
			ArchetypeWeight weights[] = {
				{ARCH_coal_depo, 2},
				{ARCH_rock_small, 3},
				{ARCH_rock_medium, 1},
				{ARCH_rock_large, 0.5},
				{ARCH_meteorite_depo, 0.5},
			};
			ArchetypeID arch = random_weighted_archetype(weights, ARRAY_COUNT(weights));
			if (meteorite_count == 0) {
				arch = ARCH_meteorite_depo;
				meteorite_count += 1;
			}

			Vector2 spawn_pos;
			bool found_pos = false;
			int iterations = 0;
			while (true) {
				spawn_pos = en->pos;
				spawn_pos.x += get_random_float32_in_range(en->radius * -0.5, en->radius * 0.5);
				spawn_pos.y += get_random_float32_in_range(en->radius * -0.5, en->radius * 0.5);
				spawn_pos = snap_position_to_nearest_tile_based_on_arch(spawn_pos, arch);

				if (!does_overlap_existing_entities(spawn_pos, arch, en->dim)) {
					found_pos = true;
					break;
				}

				iterations += 1;
				if (iterations > 100) { // failsafe
					break;
				}
			}

			if (found_pos) {
				Entity* spawn = entity_create(en->dim);
				entity_setup(spawn, arch);
				spawn->pos = spawn_pos;

				add_new_entity_to_tile_cache(spawn, en->dim); // need to update the cache instantly, because gangster shit is happening
			} else {
				log_warning("couldn't find space for spawning in a resource");
				break;
			}
		}

		entity_zero_immediately(en);
	}

}
// :meteor
void render_meteor(Entity* en) {

	float alpha = alpha_from_end_time(en->next_hit_end_time, meteor_strike_length);

	float radius = en->radius * ease_in_exp(alpha, 8);

	Vector2 draw_pos = en->pos;
	draw_pos.x -= radius;
	draw_pos.y -= radius;

	Vector4 col = COLOR_RED;
	col.a = 0.7 * alpha;

	Draw_Quad* quad = draw_circle_in_frame(draw_pos, v2(radius * 2, radius * 2), col, current_draw_frame);
	quad->z = layer_background+1;

}

// :turret
void update_turret(Entity* en) {

	en->radius = 100.f;

	// find closest
	Entity* closest_enemy = 0;
	float closest_enemy_dist = 0;
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* against = &world->entities[i];
		if (against->is_valid && against != en && against->is_enemy) {

			float dist = v2_dist(against->pos, en->pos);
			if (dist > en->radius) {
				continue;
			}

			if (!closest_enemy || dist < closest_enemy_dist) {
				closest_enemy_dist = dist;
				closest_enemy = against;
			}
		}
	}

	// shoot
	if (closest_enemy && has_reached_end_time(en->next_hit_end_time)) {
		en->next_hit_end_time = now() + 0.5;

		Vector2 shoot_dir = v2_normalize(v2_sub(closest_enemy->pos, en->pos));
		en->last_shoot_dir = shoot_dir;

		if (en->input0.id) {
			if (en->input0.amount <= 2) {
				play_sound_at_pos("event:/turret_shot_low", en->pos);
			} else {
				play_sound_at_pos("event:/turret_shot", en->pos);
			}

			closest_enemy->velocity = v2_add(closest_enemy->velocity, v2_mulf(shoot_dir, 800));
			en->frame.did_shoot = true;
			en->input0.amount -= 1;
			if (en->input0.amount <= 0) {
				en->input0 = (ItemInstanceData){0};
			}

			{
				Vector2 pos = closest_enemy->pos;
				Vector4 col = COLOR_RED;

				for (int i = 0; i < 10; i++) {
					Particle* p = particle_new();
					p->flags |= PARTICLE_FLAGS_physics | PARTICLE_FLAGS_friction | PARTICLE_FLAGS_fade_out_with_velocity | PARTICLE_FLAGS_light;
					p->pos = pos;
					pos.x += get_random_float32_in_range(-2, 2);
					pos.y += get_random_float32_in_range(-2, 2);
					p->velocity = v2_mulf(shoot_dir, get_random_float32_in_range(200, 300));
					p->col = col;
					p->friction = 20.0f;
					p->fade_out_vel_range = 30.0f;

					p->light_col = col;
					p->light_intensity = 0.3;
					p->light_radius = 2;
				}
			}

			closest_enemy->health -= 20;
			if (closest_enemy->health <= 0) {
				// kill
				do_entity_drops(closest_enemy);
				do_entity_exp_drops(closest_enemy);
				entity_zero_immediately(closest_enemy);
			}
		} else {
			play_sound_at_pos("event:/no_ammo_click", en->pos);
		}
	}
}

void render_turret(Entity* en) {
	draw_base_sprite(en);

	Vector2 mouse_dir = v2_normalize(v2_sub(get_mouse_pos_in_current_space(), en->pos));

	Vector2 dir = en->last_shoot_dir;
	dir.y *= -1; // for some reason this needs to be flipped.

	// angle_from_vector example
	float rotation = atan2(dir.y, dir.x);

	en->rotation_target = rotation;
	animate_f32_to_target(&en->rotation_current, en->rotation_target, delta_t, 40);

	Vector2 size = v2(12, 2);

	Matrix4 xform = m4_identity;
	xform = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
	xform = m4_rotate_z(xform, en->rotation_current);
	xform = m4_translate(xform, v3(-1, size.y * -0.5, 0));

	// muzzle flash
	if (en->frame.did_shoot) {
		Matrix4 target_xform = m4_identity;
		target_xform = m4_translate(target_xform, v3(en->pos.x, en->pos.y, 0));
		target_xform = m4_rotate_z(target_xform, en->rotation_target);
		target_xform = m4_translate(target_xform, v3(-1, size.y * -0.5, 0));
		Vector2 muzzle_pos = v2(12, 0);
		muzzle_pos = m4_transform(target_xform, v4(v2_expand(muzzle_pos), 0, 1)).xy;

		for (int i = 0; i < 7; i++) {
			Particle* p = particle_new();
			p->flags |= PARTICLE_FLAGS_light | PARTICLE_FLAGS_physics | PARTICLE_FLAGS_friction | PARTICLE_FLAGS_fade_out_with_velocity;
			p->pos = muzzle_pos;
			p->col = col_golden;
			p->friction = 40;
			p->fade_out_vel_range = 30;

			p->velocity = v2_normalize(v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1)));
			p->velocity = v2_mulf(p->velocity, get_random_float32_in_range(200, 200));

			p->light_col = col_golden;
			p->light_intensity = 0.3;
			p->light_radius = 10;
		}
	}

	draw_rect_xform_in_frame(xform, size, COLOR_BLACK, current_draw_frame);
}

// :enemy
void update_enemy(Entity* en) {

	float enemy_agro_radius = 70.f;

	// find nearest natural target
	Entity* nearest_target = 0;
	float nearest_target_distance = 0;
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* against = &world->entities[i];
		if (is_valid(against) && against->enemy_target) {

			if (against->arch == ARCH_player && !is_player_alive()) {
				continue;
			}

			float dist = v2_dist(against->pos, en->pos);
			bool should_agro = dist < enemy_agro_radius || is_night() || en->is_agro;

			if (should_agro && (!nearest_target || nearest_target_distance > dist)) {
				nearest_target = against;
				nearest_target_distance = dist;
			}

		}
	}

	Entity* target_en = get_nil_entity();
	if (nearest_target) {
		target_en = nearest_target;
		en->is_agro = true; // perma agro
	}

	en->frame.target_en = target_en;
	en->frame.is_awake = is_valid(target_en);

	if (en->frame.is_awake && !en->last_frame.is_awake) {
		attach_cont_sound_to_entity("event:/enemy_wakeup", en);
	}
	if (en->frame.is_awake) {
		update_attached_cont_sounds_entity(en);
	}

	en->friction = 20.f;
	en->move_speed = 85.f;

	if (target_en->is_valid) {

		if (v2_dist(target_en->pos, en->pos) < 16.f) {
			// attacc hard
			en->move_speed = 300.f;
		}

		// apply movement to velocity
		if (has_reached_end_time(en->movement_cooldown_end_time)) {
			en->frame.input_axis = v2_normalize(v2_sub(target_en->pos, en->pos));
			en->velocity = v2_mulf(en->frame.input_axis, en->move_speed);
		}

		// collide with target
		if (has_reached_end_time(en->movement_cooldown_end_time)) {
			Range2f target_bounds = get_entity_collision_bounds(target_en);
			target_bounds = range2f_shift(target_bounds, target_en->pos);
			Range2f our_bounds = en->collision_bounds;
			our_bounds = range2f_shift(our_bounds, en->pos);
			if (range2f_overlaps(target_bounds, our_bounds)) {
				// knockback self
				en->velocity = v2_mulf(en->frame.input_axis, -500.f);
				en->movement_cooldown_end_time = now() + 1.f;

				play_sound_at_pos("event:/enemy_hit", target_en->pos);

				if (target_en->arch == ARCH_player) {
					target_en->oxygen -= 5;
					camera_shake(0.2);

					if (target_en->oxygen <= 0) {
						en->is_agro = false;
					}
				} else {
					target_en->health -= 1;
					if (target_en->health <= 0) {
						do_entity_drops(target_en);
						entity_zero_immediately(target_en);
						en->is_agro = false;
					}
				}
			}
		}
	}
}

int compare_entity_y(const void* a, const void* b) {
	Entity* entityA = *(Entity**)a;
	Entity* entityB = *(Entity**)b;

	if (entityA->pos.y < entityB->pos.y) {
		return 1;
	} else if (entityA->pos.y > entityB->pos.y) {
		return -1;
	}

	// they're equal, so just sort by id so we don't z-fight
	if (entityA->id < entityB->id) {
		return 1;
	} else {
		return -1;
	}
}


void draw_world_in_frame(Dimension dim) {

	set_world_space();

	push_z_layer_in_frame(layer_world, current_draw_frame);

	// grab entities and sort by Y pos
	Entity** entities_to_render;
	growing_array_init_reserve((void**)&entities_to_render, sizeof(Entity*), MAX_ENTITY_COUNT, get_temporary_allocator());
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* en = &world->entities[i];
		if (!(en->is_valid && en->dim == dim)) {
			continue;
		}
		growing_array_add((void**)&entities_to_render, &en);
	}
	qsort(entities_to_render, growing_array_get_valid_count(entities_to_render), sizeof(Entity*), compare_entity_y);

	// render entities
	tm_scope("push render entities")
	for (int i = 0; i < growing_array_get_valid_count(entities_to_render); i++)
	{
		Entity* en = entities_to_render[i];

		if (en->is_item) {
			if (en->arch == ARCH_exp) {
				draw_rect_in_frame(en->pos, v2(1, 1), col_exp, current_draw_frame);
			} else {
				// try drawing the item's icon
				SpriteID id = en->icon;
				if (!id) {
					id=en->sprite_id;
				}

				Vector2 pos = en->pos;
				pos.y += 2.0 * sin_breathe(os_get_elapsed_seconds(), 5.0);

				draw_sprite_at_pos_pivot(id, pos, PIVOT_center_center);
			}
		} else {
			switch (en->arch) {

				// :render
				case ARCH_player: render_player(en); break;
				case ARCH_portal: if (world_frame.draw_portals) render_portal(en); break;
				case ARCH_extractor:
				case ARCH_conveyor: render_conveyor(en); break;
				case ARCH_enemy_nest: render_enemy_nest(en); break;
				case ARCH_meteor: render_meteor(en); break;
				case ARCH_turret: render_turret(en); break;

				// :enemy
				case ARCH_enemy1: {
					Vector2 center = en->pos;
					float rate_mult = en->frame.target_en->is_valid ? 1.f : 0;
					center.y += 2.f * sin_breathe(os_get_elapsed_seconds(), 40.0 * rate_mult);
					center.x += 1.f * sin_breathe(os_get_elapsed_seconds(), 80.0 * rate_mult);

					Vector2 size = enemy_size;
					Vector2 draw_pos = center;
					draw_pos.x += size.x * -0.5;
					draw_pos.y += size.y * -0.5;

					Vector4 col = COLOR_RED;
					if (!en->frame.target_en->is_valid) {
						col = v4_mul(col, v4(0.5, 0.5, 0.5, 1.f));
					}

					draw_rect_in_frame(draw_pos, size, col, current_draw_frame);

					if (en->frame.target_en->is_valid) {
						add_point_light(center, v4(1,0,0,0.5), 20, 1);
					}

				} break;

				default: {
					draw_base_sprite(en);
				} break;
			}
		}

		// :oxygenerator render
		if (en->arch == ARCH_oxygenerator) {
			Vector2 size = {2, 5};
			Vector2 draw_pos = en->pos;
			draw_pos.x -= size.x * 0.5;
			draw_pos.y -= 3;

			size.y = size.y * ((float)en->oxygen / (float)en->oxygen_max);

			draw_rect_in_frame(draw_pos, v2(size.x, size.y), col_oxygen, current_draw_frame);
		}

		// :health bar
		if (en->health && en->health < en->max_health) {
			Vector2 size = {6, 1};
			Vector2 draw_pos = en->pos;
			draw_pos.x -= size.x * 0.5;

			draw_pos.y += get_offset_for_rendering(en->arch).y;
			draw_pos.y -= 2;

			draw_rect_in_frame(draw_pos, size, COLOR_BLACK, current_draw_frame);

			float target_alpha = (float)en->health / (float)en->max_health;

			if (en->health_bar_current_alpha == 0.0) {
				en->health_bar_current_alpha = 1.0;
			}
			animate_f32_to_target(&en->health_bar_current_alpha, target_alpha, delta_t, 30.0f);

			draw_rect_in_frame(draw_pos, v2(size.x * en->health_bar_current_alpha, size.y), COLOR_WHITE, current_draw_frame);
		}

		// :tether draw blue thingy
		if (en->arch == ARCH_tether && en->is_oxygen_tether && en->frame.is_powered) {
			Vector2 draw_pos = v2_add(en->pos, v2(-1, -1));
			draw_pos = v2_add(draw_pos, en->tether_connection_offset);
			draw_rect_in_frame(draw_pos, v2(2, 2), col_oxygen, current_draw_frame);
		}
	}

	// :tile :rendering
	scope_z_layer(layer_background)
	{
		int player_tile_x = world_pos_to_tile_pos(world_frame.camera_pos_copy.x);
		int player_tile_y = world_pos_to_tile_pos(world_frame.camera_pos_copy.y);
		int tile_radius_x = 40;
		int tile_radius_y = 30;
		for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; x++) {
			for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; y++) {

				BiomeID biome = biome_at_tile(v2i(x, y), dim);
				if (biome == 0) {
					continue;
				}

				if (dim == DIM_first) {

					// checkerboard pattern
					Vector4 col = color_0;
					if ((x + (y % 2 == 0) ) % 2 == 0) {
						col.a = 0.9;
					}
					col = v4_lerp(col, biome_col_hex_to_rgba(biome_colors[biome]), 0.1f);
					float x_pos = x * tile_width;
					float y_pos = y * tile_width;
					Draw_Quad* quad = draw_rect_in_frame(v2(x_pos, y_pos), v2(tile_width, tile_width), col, current_draw_frame);

				} else if (dim == DIM_second) {

					Vector4 col = hex_to_rgba(0x8eb149ff);

					float x_pos = x * tile_width;
					float y_pos = y * tile_width;
					Draw_Quad* quad = draw_rect_in_frame(v2(x_pos, y_pos), v2(tile_width, tile_width), col, current_draw_frame);

				}
			}
		}

		// draw_rect_in_frame(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5), current_draw_frame);
	}

	tm_scope("particle render") {
		particle_render();
	}

	// player :hud
	if (is_player_alive() && get_player_dim() == dim)
	{
		Entity* player = get_player();

		// o2 meter
		{
			Vector2 size = {6, 1};
			Vector2 draw_pos = player->pos;
			draw_pos.x -= size.x * 0.5;
			draw_pos.y -= 6.0;
			draw_rect_in_frame(draw_pos, size, COLOR_BLACK, current_draw_frame);
			float alpha = (float)player->oxygen / (float)player->oxygen_max;
			draw_rect_in_frame(draw_pos, v2(size.x * alpha, size.y), col_oxygen, current_draw_frame);
		}
	}

	// :shader cbuffer update
	{
		#if CONFIGURATION == DEBUG
		if (is_key_just_pressed('U')) {
			world->night_alpha_target = world->night_alpha_target == 0 ? 1.0 : 0.0;
			log("%f", world->night_alpha_target);
		}
		#endif

		cbuffer.night_alpha = world->night_alpha;
		// cbuffer.night_alpha = sin_breathe(world->time_elapsed, 2.f);
		// log("%f", cbuffer.night_alpha);

		// player light
		add_point_light(get_player()->pos, v4(0,0,0,0), 100, 1);
	}
}

// :entry
int entry(int argc, char **argv) {
	window.title = STR("Randy's Game");
	window.width = 1920;
	window.height = 1080;
	window.clear_color = COLOR_BLACK;
	window.force_topmost = false;

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	// :init

	seed_for_random = rdtsc();

	// :sound init
	fmod_init();
	#if defined(LOOP_SOUND)
	play_sound("event:/bg_loop");
	#endif

	// :col
	col_golden = hex_to_rgba(0xddaa47ff);
	col_fire = hex_to_rgba(0xf66144ff);
	col_exp = hex_to_rgba(0x7bd47aff);
	col_select = col_exp;
	color_0 = hex_to_rgba(0x2a2d3aff);
	col_oxygen = hex_to_rgba(0xaad9e6ff);
	col_tether = col_oxygen;
	col_tether.a = 0.5;

	// sprite setup
	{
		sprites[0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/missing_tex.png"), get_heap_allocator()) };
		// sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/player.png"), get_heap_allocator()) };
		sprites[SPRITE_tree0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree0.png"), get_heap_allocator()) };
		sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree1.png"), get_heap_allocator()) };
		sprites[SPRITE_rock0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/rock0.png"), get_heap_allocator()) };
		sprites[SPRITE_item_pine_wood] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_pine_wood.png"), get_heap_allocator()) };
		sprites[SPRITE_item_rock] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_rock.png"), get_heap_allocator()) };
		sprites[SPRITE_furnace] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/furnace.png"), get_heap_allocator()) };
		sprites[SPRITE_workbench] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/workbench.png"), get_heap_allocator()) };
		sprites[SPRITE_research_station] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/research_station.png"), get_heap_allocator()) };
		sprites[SPRITE_exp] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/exp.png"), get_heap_allocator()) };
		sprites[SPRITE_exp_vein] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/exp_vein.png"), get_heap_allocator()) };
		sprites[SPRITE_copper_depo] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/copper_depo.png"), get_heap_allocator()) };
		sprites[SPRITE_raw_copper] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/raw_copper.png"), get_heap_allocator()) };
		sprites[SPRITE_copper_ingot] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/copper_ingot.png"), get_heap_allocator()) };
		sprites[SPRITE_fiber] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/fiber.png"), get_heap_allocator())};
		sprites[SPRITE_flint] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint.png"), get_heap_allocator())};
		sprites[SPRITE_flint_axe] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_axe.png"), get_heap_allocator())};
		sprites[SPRITE_flint_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_depo.png"), get_heap_allocator())};
		sprites[SPRITE_flint_pickaxe] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_pickaxe.png"), get_heap_allocator())};
		sprites[SPRITE_flint_scythe] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_scythe.png"), get_heap_allocator())};
		sprites[SPRITE_grass] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/grass.png"), get_heap_allocator())};
		sprites[SPRITE_coal] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/coal.png"), get_heap_allocator())};
		sprites[SPRITE_oxygenerator] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/oxygenerator.png"), get_heap_allocator())};
		sprites[SPRITE_tether] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/tether.png"), get_heap_allocator())};
		sprites[SPRITE_o2_shard] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/o2_shard.png"), get_heap_allocator())};
		sprites[SPRITE_ice_vein] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/ice_vein.png"), get_heap_allocator())};
		sprites[SPRITE_player_walk] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/player_walk.png"), get_heap_allocator()), .frames=4};
		sprites[SPRITE_player_idle] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/player_idle.png"), get_heap_allocator()), .frames=1};
		sprites[SPRITE_ice_tile] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/ice_tile.png"), get_heap_allocator())};
		sprites[SPRITE_burner_drill] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/burner_drill.png"), get_heap_allocator())};
		sprites[SPRITE_longboi_test] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/longboi_test.png"), get_heap_allocator())};
		sprites[SPRITE_coal_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/coal_depo.png"), get_heap_allocator())};
		sprites[SPRITE_iron_ingot] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/iron_ingot.png"), get_heap_allocator())};
		sprites[SPRITE_raw_iron] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/raw_iron.png"), get_heap_allocator())};
		sprites[SPRITE_iron_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/iron_depo.png"), get_heap_allocator())};
		sprites[SPRITE_wall] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/wall.png"), get_heap_allocator())};
		sprites[SPRITE_wall_gate] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/wall_gate.png"), get_heap_allocator())};
		sprites[SPRITE_fabricator] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/fabricator.png"), get_heap_allocator())};
		sprites[SPRITE_o2_emitter] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/o2_emitter.png"), get_heap_allocator())};
		sprites[SPRITE_turret] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/turret.png"), get_heap_allocator())};
		sprites[SPRITE_bullet] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/bullet.png"), get_heap_allocator())};
		sprites[SPRITE_enemy_nest] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/enemy_nest.png"), get_heap_allocator())};
		sprites[SPRITE_red_core] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/red_core.png"), get_heap_allocator())};
		sprites[SPRITE_large_ice_vein] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/large_ice_vein.png"), get_heap_allocator())};
		sprites[SPRITE_wood_crate] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/wood_crate.png"), get_heap_allocator())};
		sprites[SPRITE_conveyor_up] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/conveyor_up.png"), get_heap_allocator())};
		sprites[SPRITE_conveyor_down] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/conveyor_down.png"), get_heap_allocator())};
		sprites[SPRITE_conveyor_left] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/conveyor_left.png"), get_heap_allocator())};
		sprites[SPRITE_conveyor_right] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/conveyor_right.png"), get_heap_allocator())};
		sprites[SPRITE_large_coal_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/large_coal_depo.png"), get_heap_allocator())};
		sprites[SPRITE_anti_meteor] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/anti_meteor.png"), get_heap_allocator())};
		sprites[SPRITE_portal_icon] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/portal_icon.png"), get_heap_allocator())};
		sprites[SPRITE_portal_frame] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/portal_frame.png"), get_heap_allocator())};
		sprites[SPRITE_large_iron_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/large_iron_depo.png"), get_heap_allocator())};
		sprites[SPRITE_extractor_east] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/extractor_east.png"), get_heap_allocator())};
		sprites[SPRITE_extractor_west] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/extractor_west.png"), get_heap_allocator())};
		sprites[SPRITE_extractor_north] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/extractor_north.png"), get_heap_allocator())};
		sprites[SPRITE_extractor_south] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/extractor_south.png"), get_heap_allocator())};
		sprites[SPRITE_thumper] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/thumper.png"), get_heap_allocator())};
		sprites[SPRITE_rock_small] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/rock_small.png"), get_heap_allocator())};
		sprites[SPRITE_rock_medium] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/rock_medium.png"), get_heap_allocator())};
		sprites[SPRITE_rock_large] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/rock_large.png"), get_heap_allocator())};
		sprites[SPRITE_meteorite_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/meteorite_depo.png"), get_heap_allocator())};
		sprites[SPRITE_raw_meteorite] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/raw_meteorite.png"), get_heap_allocator())};
		sprites[SPRITE_meteorite_ingot] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/meteorite_ingot.png"), get_heap_allocator())};
		sprites[SPRITE_blank_tp_focus] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/blank_tp_focus.png"), get_heap_allocator())};
		sprites[SPRITE_charged_tp_focus] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/charged_tp_focus.png"), get_heap_allocator())};
		sprites[SPRITE_portal_controller] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/portal_controller.png"), get_heap_allocator())};
		// :sprite

		#if CONFIGURATION == DEBUG
		{
			for (SpriteID i = 0; i < SPRITE_MAX; i++) {
				Sprite* sprite = &sprites[i];
				assert(sprite->image, "Sprite was not setup properly");
			}
		}
		#endif
	}

	font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());

	setup_entity_archetype_data_cache();

	init_biome_maps();

	// the :init zone

	// :shader init
	shader_recompile();

	// world load / setup
	if (os_is_file_s(STR("world"))) {
		bool succ = world_attempt_load_from_disk();
		if (!succ) {
			// just setup a new world if it fails
			world_setup();
		}
	} else {
		world_setup();
	}
	world_save_to_disk();

	Draw_Frame offscreen_draw_frame;
	draw_frame_init(&offscreen_draw_frame);

	Draw_Frame ui_draw_frame;
	draw_frame_init(&ui_draw_frame);

	float64 seconds_counter = 0.0;

	float64 last_time = os_get_elapsed_seconds();
	
	// :loop
	while (!window.should_close)
	tm_scope("frame")
	{
		reset_temporary_storage();
		world_frame = (WorldFrame){0};
		float64 current_time = os_get_elapsed_seconds();
		delta_t = current_time - last_time;
		last_time = current_time;
		os_update();
		current_draw_frame = 0;

		local_persist Gfx_Image *game_image = 0;
		local_persist Gfx_Image *ui_image = 0;
		local_persist Os_Window last_window;
		if ((last_window.width != window.width || last_window.height != window.height || !game_image) && window.width > 0 && window.height > 0) {
			if (game_image)  delete_image(game_image);
			if (ui_image)  delete_image(ui_image);
			
			game_image = make_image_render_target(window.width, window.height, 4, 0, get_heap_allocator());
			ui_image = make_image_render_target(window.width, window.height, 4, 0, get_heap_allocator());
		}
		last_window = window;

		// zero entity frame state
		for (int i = 0; i < MAX_ENTITY_COUNT; i++)
		{
			Entity* en = &world->entities[i];
			if (en->is_valid) {
				en->last_frame = en->frame;
				en->frame = (EntityFrame){0};
			}
		}

		// reset appframe
		last_app_frame = app_frame;
		app_frame = (AppFrame){0};

		// :world update
		tm_scope("world update")
		{
			world->tick_count += 1;
			world->time_elapsed += delta_t;

			// setup tile entity cache for the frame
			create_tile_entity_pair_cache();

			// find player lol
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid && en->arch == ARCH_player) {
					world_frame.player = en;
				}
			}

			do_resource_respawning();
		}

		// debug adjust zoom
		#if CONFIGURATION == DEBUG
		{
			if (is_key_down(KEY_SHIFT)) {
				if (is_key_down('Q')) {
					camera_zoom += 0.1;
				}
				if (is_key_down('E')) {
					camera_zoom -= 0.1;
					camera_zoom = clamp_bottom(camera_zoom, 0.4);
				}
			}
		}
		#endif

		// :input
		if (is_key_just_pressed(KEY_F11)) {
			consume_key_just_pressed(KEY_F11);
			window.fullscreen = !window.fullscreen;
		}

		// :player input axis
		if (is_player_alive()) {
			Vector2 input_axis = v2(0, 0);
			if (is_key_down('A')) {
				input_axis.x -= 1.0;
			}
			if (is_key_down('D')) {
				input_axis.x += 1.0;
			}
			if (is_key_down('S')) {
				input_axis.y -= 1.0;
			}
			if (is_key_down('W')) {
				input_axis.y += 1.0;
			}
			input_axis = v2_normalize(input_axis);
			Entity* player = get_player();
			if (input_axis.x != 0) {
				player->last_move_dir.x = extract_sign(input_axis.x);
			}
			if (input_axis.y != 0) {
				player->last_move_dir.y = extract_sign(input_axis.y);
			}
			player->frame.input_axis = input_axis;
		}

		// :frame update
		world_frame.world_proj = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		// :camera
		{
			// camera shake - https://www.youtube.com/watch?v=tu-Qe66AvtY
			camera_trauma -= delta_t;
			camera_trauma = clamp_bottom(camera_trauma, 0);

			Vector2 target_pos = get_player()->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f);

			set_world_view();
		}

		// this is kinda yuck...
		world_frame.screen_proj = m4_make_orthographic_projection(0.0, screen_width, 0.0, screen_height, -1, 10);
		world_frame.screen_view = m4_identity;

		world_frame.render_target_h = window.height;
		world_frame.render_target_w = window.width;

		if (world->ux_state == UX_entity_interaction) {
			world_frame.show_inventory = true;
		}

		// :ui draw to image
		{
			draw_frame_reset(&ui_draw_frame);
			gfx_clear_render_target(ui_image, v4(0,0,0,0));

			ui_draw_frame.enable_z_sorting = true;
			ui_draw_frame.shader_extension = global_shader;
			ui_draw_frame.cbuffer = &(ShaderConstBuffer){0};
			current_draw_frame = &ui_draw_frame;

			do_ui_stuff();
			do_world_entity_interaction_ui_stuff();
			do_cursor_drawing_and_logic();

			// esc exit
			if (world->ux_state != UX_nil && world->ux_state != UX_respawn && is_key_just_pressed(KEY_ESCAPE)) {
				consume_key_just_pressed(KEY_ESCAPE);
				world->ux_state = 0;
			}

			current_draw_frame = 0;
		}

		// :tether stuff
		{
			// for each tether, find all nearby tethers
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* self_tether = &world->entities[i];
				if (self_tether->is_valid && self_tether->is_oxygen_tether) {

					Entity** nearby_tethers;
					growing_array_init_reserve((void**)&nearby_tethers, sizeof(Entity*), 1, get_temporary_allocator());
					for (int j = 0; j < MAX_ENTITY_COUNT; j++) {
						Entity* nearby_tether = &world->entities[j];
						if (!(nearby_tether->is_valid && nearby_tether->dim == self_tether->dim)) {
							continue;
						}
						if (nearby_tether != self_tether && nearby_tether->is_valid && nearby_tether->is_oxygen_tether) {
							if (v2_dist(nearby_tether->pos, self_tether->pos) < tether_connection_radius) {
								growing_array_add((void**)&nearby_tethers, &nearby_tether);
							}
						}
					}

					self_tether->frame.connected_to_tethers = nearby_tethers;
				}
			}

			// run through connections recursively, starting at the core tether
			Entity* oxygenerator = entity_from_handle(world->oxygenerator);
			if (oxygenerator->oxygen > 0)
			{
				Entity** connection_stack;
				growing_array_init_reserve((void**)&connection_stack, sizeof(Entity*), 1, get_temporary_allocator());
				growing_array_add((void**)&connection_stack, &oxygenerator);

				while (growing_array_get_valid_count(connection_stack)) {
					Entity* current = connection_stack[growing_array_get_valid_count(connection_stack)-1];
					growing_array_pop((void**)&connection_stack);

					for (int i = 0; i < growing_array_get_valid_count(current->frame.connected_to_tethers); i ++) {
						Entity* connected_tether = current->frame.connected_to_tethers[i];
						if (!connected_tether->frame.is_powered) {
							growing_array_add((void**)&connection_stack, &connected_tether);
							connected_tether->frame.is_powered = true;
							draw_line(v2_add(connected_tether->pos, connected_tether->tether_connection_offset), v2_add(current->pos, current->tether_connection_offset), 1.0f, col_tether);
						}
					}
				}
			}
		}

		// for each o2 emitter, run through neighboring wall seals, making them powered
		tm_scope("power algo")
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid && en->arch == ARCH_o2_emitter && en->frame.is_powered) {

				Entity** stack;
				growing_array_init_reserve((void**)&stack, sizeof(Entity*), 1, get_temporary_allocator());
				growing_array_add((void**)&stack, &en);

				while (growing_array_get_valid_count(stack)) {
					Entity* current = stack[growing_array_get_valid_count(stack)-1];
					growing_array_pop((void**)&stack);

					current->frame.is_powered = true;
					Tile current_tile = v2_world_pos_to_tile_pos(current->pos);

					Entity* left = entity_at_tile(v2i_add(v2i(-1, 0), current_tile), DIM_first);
					if (left && left->wall_seal && !left->frame.is_powered) {
						growing_array_add((void**)&stack, &left);
					}

					Entity* right = entity_at_tile(v2i_add(v2i(1, 0), current_tile), DIM_first);
					if (right && right->wall_seal && !right->frame.is_powered) {
						growing_array_add((void**)&stack, &right);
					}

					Entity* top = entity_at_tile(v2i_add(v2i(0, 1), current_tile), DIM_first);
					if (top && top->wall_seal && !top->frame.is_powered) {
						growing_array_add((void**)&stack, &top);
					}

					Entity* bottom = entity_at_tile(v2i_add(v2i(0, -1), current_tile), DIM_first);
					if (bottom && bottom->wall_seal && !bottom->frame.is_powered) {
						growing_array_add((void**)&stack, &bottom);
					}
				}
			}
		}

		// figure out if the player is inside
		bool is_inside = true;
		tm_scope("room flood fill")
		{
			Entity* player = get_player();
			Dimension dim = get_player_dim();

			Vector2i* stack;
			growing_array_init_reserve((void**)&stack, sizeof(Vector2i), maps[dim].width*maps[dim].height, get_temporary_allocator());

			Vector2i start_tile = v2_world_pos_to_tile_pos(player->pos);
			growing_array_add((void**)&stack, &start_tile);

			while (growing_array_get_valid_count(stack)) {
				Vector2i self = stack[growing_array_get_valid_count(stack)-1];
				growing_array_pop((void**)&stack);

				TileCache* self_tc = tile_cache_at_tile(self, dim);
				self_tc->visited = true;

				for (int i = 0; i < 4; i++) {
					Vector2i tile = self;
					if (i == 0) {
						tile = v2i_add(tile, v2i(-1, 0));
					} else if (i == 1) {
						tile = v2i_add(tile, v2i(1, 0));
					} else if (i == 2) {
						tile = v2i_add(tile, v2i(0, 1));
					} else if (i == 3) {
						tile = v2i_add(tile, v2i(0, -1));
					}

					Vector2i dist = v2i_sub(tile, start_tile);
					float length = v2i_length(dist);
					if (length > 40) {
						is_inside = false;
						continue;
					}

					TileCache* tile_cache = tile_cache_at_tile(tile, dim);
					bool is_wall_seal = tile_cache->entity && tile_cache->entity->wall_seal && tile_cache->entity->frame.is_powered;
					if (!tile_cache->visited && !is_wall_seal) {
						growing_array_add((void**)&stack, &tile);
					}
				}
			}
		}

		// :select entity
		if (!world_frame.hover_consumed)
		{
			float smallest_dist = 99999;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (!(en->is_valid && en->dim == get_player_dim())) {
					continue;
				}

				bool has_interaction = en->destroyable_world_item
					|| en->big_resource_drop
					|| en->interactable_entity
					|| en->right_click_remove;
				// add extra :interact cases here ^^

				float dist_to_player = v2_dist(en->pos, get_player()->pos);
				if (en->is_valid && has_interaction) {
					Sprite* sprite = get_sprite(en->sprite_id);

					int entity_tile_x = world_pos_to_tile_pos(en->pos.x);
					int entity_tile_y = world_pos_to_tile_pos(en->pos.y);

					float dist = fabsf(v2_dist(en->pos, get_mouse_pos_in_current_space()));
					if (dist < cursor_selection_slop_radius) {
						if (!world_frame.selected_entity || (dist < smallest_dist)) {
							world_frame.selected_entity = en;
							smallest_dist = dist;
						}
					}
				}
			}

			// :select entity UI
			{
				current_draw_frame = &ui_draw_frame;

				if (world_frame.selected_entity) {
					Entity* en = world_frame.selected_entity;

					// draw basic tooltip
					defer_scope(set_screen_space(), set_world_space())
					scope_z_layer(layer_ui)
					{
						float x0 = screen_width * 0.5;
						float y0 = screen_height;
						y0 -= 2.f;

						string txt = en->pretty_name;
						Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), v2(0.1, 0.1), COLOR_WHITE, PIVOT_top_center);
						y0 -= met.visual_size.y;
						y0 -= 2;

						if (en->last_frame.error) {
							switch (en->last_frame.error) {
								case GAME_ERR_no_fuel: txt = STR("Needs fuel"); break;
								case GAME_ERR_no_room_in_destination: txt = STR("No room in destination"); break;
								case GAME_ERR_no_o2: txt = STR("Needs oxygen network connection"); break;
								default: txt = STR("");
							}
							draw_text_with_pivot(font, txt, font_height, v2(x0, y0), v2(0.1, 0.1), v4_lerp(COLOR_WHITE, COLOR_RED, get_error_flash_frequency()), PIVOT_top_center);
						}
					}

					// draw radius stuff 
					if (en->arch != ARCH_oxygenerator && en->radius)
					{
						Vector2 draw_pos = en->pos;
						draw_pos.x -= en->radius;
						draw_pos.y -= en->radius;
						draw_circle_in_frame(draw_pos, v2(en->radius * 2, en->radius * 2), v4(1, 1, 1, 0.05), current_draw_frame);
					}

					// todo - draw selection corners like factorio
					// Range2f range = get_entity_range(en);
				}

				current_draw_frame = 0;
			}

		}

		// spawn :meteors
		{
			Entity** anti_meteor_entities;
			growing_array_init_reserve((void**)&anti_meteor_entities, sizeof(Entity*), 1, get_temporary_allocator());
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid && en->has_anti_meteor_radius && en->radius != 0 && en->last_frame.is_powered) {
					growing_array_add((void**)&anti_meteor_entities, &en);
				}
			}

			// close spawn
			// this is so we basically whack the player lol
			{
				if (world->next_close_meteor_spawn_end_time == 0) {
					world->next_close_meteor_spawn_end_time = now() + get_random_float32_in_range(100, 120);
					// world->next_close_meteor_spawn_end_time = now() + 1.0f;
				}

				if (has_reached_end_time(world->next_close_meteor_spawn_end_time)) {
					world->next_close_meteor_spawn_end_time = 0;

					float meteor_radius = get_archetype_data(ARCH_meteor).radius;

					Vector2 spawn_pos;
					bool found_pos = false;
					int iterations = 0;
					while (true) {

						float max_spawn_radius = 0.f;
						if (iterations > 10) {
							max_spawn_radius = 150.f;
						}

						spawn_pos = get_player()->pos;
						spawn_pos = v2_add(spawn_pos, v2_mulf(get_random_v2(), get_random_float32_in_range(0, max_spawn_radius)));

						bool is_in_no_go_zone = false;
						for (int i = 0; i < growing_array_get_valid_count(anti_meteor_entities); i++) {
							Entity* en = anti_meteor_entities[i];
							float dist = v2_dist(en->pos, spawn_pos);
							if (dist < en->radius + meteor_radius) {
								is_in_no_go_zone = true;
							}
						}

						if (!is_in_no_go_zone) {
							found_pos = true;
							break;
						}

						iterations += 1;
						if (iterations > 100) {
							break;
						}
					}

					if (found_pos) {
						Entity* en = entity_create(get_player_dim());
						setup_meteor(en);
						en->pos = spawn_pos;
					} else {
						log_warning("failed to find spawn pos for meteor");
					}
				}
			}

			// far spawn
			{
				if (world->next_far_meteor_spawn_end_time == 0) {
					world->next_far_meteor_spawn_end_time = now() + get_random_float32_in_range(30, 50);
					// world->next_far_meteor_spawn_end_time = now() + 1.0f;
				}

				if (has_reached_end_time(world->next_far_meteor_spawn_end_time)) {
					world->next_far_meteor_spawn_end_time = 0;

					float meteor_radius = get_archetype_data(ARCH_meteor).radius;

					Vector2 spawn_pos;
					bool found_pos = false;
					int iterations = 0;
					while (true) {

						float start_radius = 150.f;
						float end_radius = get_world_radius(get_player_dim());
						spawn_pos = v2_mulf(get_random_v2(), get_random_float32_in_range(start_radius, end_radius));

						bool is_in_no_go_zone = false;
						for (int i = 0; i < growing_array_get_valid_count(anti_meteor_entities); i++) {
							Entity* en = anti_meteor_entities[i];
							float dist = v2_dist(en->pos, spawn_pos);
							if (dist < en->radius + meteor_radius) {
								is_in_no_go_zone = true;
							}
						}

						// don't be near player. this is a far meteor.
						if (v2_dist(get_player()->pos, spawn_pos) < 250.f) {
							is_in_no_go_zone = true;
						}

						if (!is_in_no_go_zone) {
							found_pos = true;
							break;
						}

						iterations += 1;
						if (iterations > 100) {
							break;
						}
					}

					if (found_pos) {
						Entity* en = entity_create(get_player_dim());
						setup_meteor(en);
						en->pos = spawn_pos;
					} else {
						log_warning("failed to find spawn pos for meteor");
					}
				}
			}
		}

		// nests need to be updated prior, beacuse they spawn the enemies
		// enemies need to be updated later to ensure there's at least a nil target, otherwise we crash
		tm_scope("enemy update")
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {
				if (en->arch == ARCH_enemy_nest) {
					update_enemy_nest(en);
				}
			}
		}

		// update portal controllers before the portals.
		// They stuff their items into the surrounding portals.
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (!(en->is_valid && en->arch == ARCH_portal_controller)) continue;
			update_portal_controller(en);
		}

		// update entities
		Entity* player = get_player();
		tm_scope("entity update")
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {

				switch (en->arch) {

					case ARCH_anti_meteor: {
						if (!en->frame.is_powered) {
							en->frame.error = GAME_ERR_no_o2;
						}
					} break;

					// :update
					case ARCH_portal: update_portal(en); break;
					case ARCH_thumper: update_thumper(en); break;
					case ARCH_extractor: update_extractor(en); break;
					case ARCH_conveyor: update_conveyor(en); break;
					case ARCH_meteor: update_meteor(en); break;
					case ARCH_turret: update_turret(en); break;
					case ARCH_enemy1: update_enemy(en); break;

					default: break;
				}

				// :furnace update
				if (en->arch == ARCH_furnace) {

					StorageSlot* fuel_slot = &en->storage_slots[0];
					StorageSlot* resource_slot = &en->storage_slots[1];

					// fuel consume
					if (en->current_fuel <= 0 && fuel_slot->item.id && resource_slot->item.id) {
						en->last_fuel_max = 200;
						en->current_fuel = 200;

						fuel_slot->item.amount -= 1;
						if (fuel_slot->item.amount <= 0) {
							fuel_slot->item = (ItemInstanceData){0};
						}
					}

					// consume crafting item to start
					if (!en->current_crafting_item && en->current_fuel && resource_slot->item.id) {
						en->current_crafting_item = resource_slot->item.id;

						resource_slot->item.amount -= 1;
						if (resource_slot->item.amount <= 0) {
							resource_slot->item = (ItemInstanceData){0};
						}
					}

					// progress the crafting
					if (has_reached_end_time(en->next_crafting_progress_tick_end_time) && en->current_crafting_item && en->current_fuel > 0) {
						en->progress_on_crafting += 1;
						en->current_fuel -= 1;
						en->next_crafting_progress_tick_end_time = now() + 0.1;
						if (en->progress_on_crafting >= 100) {

							Entity item_data = get_item_data(en->current_crafting_item);

							// :CRAFT!
							Entity* drop = entity_create(en->dim);
							setup_item(drop, item_data.furnace_transform_into);
							drop->pos = en->pos;
							drop->pos = v2_add(drop->pos, v2(get_random_float32_in_range(-2, 2), get_random_float32_in_range(-2, 2)));
							drop->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.1, 0.3);

							en->current_crafting_item = 0;
							en->progress_on_crafting = 0;
						}
					}

				}

				// :burner update
				if (en->arch == ARCH_burner_drill) {

					// fuel processing
					if (en->current_fuel <= 0) {
						// attempt fuel consume
						if (en->input0.id) {
							en->last_fuel_max = 50;
							en->current_fuel = 50;

							en->input0.amount -= 1;
							if (en->input0.amount <= 0) {
								en->input0 = (ItemInstanceData){0};
							}
						}
					}

					en->progress_max = 10;

					// hit timer processing
					if (en->current_fuel > 0) {
						if (en->next_hit_end_time == 0) {
							en->next_hit_end_time = now() + 1.0f;
						}
						if (en->output0.amount < 2) {
							if (has_reached_end_time(en->next_hit_end_time)) {
								en->next_hit_end_time = 0;

								en->current_fuel -= 1;

								// get tile next to it
								Tile tile_pos = v2_world_pos_to_tile_pos(en->pos);
								tile_pos.x += 1;
								TileCache* tc = tile_cache_at_tile(tile_pos, en->dim);
								if (tc->entity && tc->entity->big_resource_drop) {
									Entity* resource = tc->entity;

									en->progress += 1;
									if (en->progress >= en->progress_max) {
										en->output0.id = resource->big_resource_drop;
										en->output0.amount += 1;
										en->progress = 0;
									}
								}
							}
						} else {
							en->frame.error = GAME_ERR_no_room_in_destination;
						}
					} else {
						en->frame.error = GAME_ERR_no_fuel;
					}

					// attempt put output slot into inventory beside it.
					if (en->output0.id) {
						Tile tile_pos = v2_world_pos_to_tile_pos(en->pos);
						tile_pos.x -= 1;
						TileCache* tc = tile_cache_at_tile(tile_pos, en->dim);
						if (tc->entity && tc->entity->has_input_storage) {
							Entity* storage = tc->entity;

							if (storage->arch == ARCH_conveyor) {
								if (storage->input0.id == 0) {
									storage->input0.amount = 1;
									storage->input0.id = en->output0.id;
									en->output0.amount -= 1;
									if (en->output0.amount <= 0) {
										en->output0 = (ItemInstanceData){0};
									}
								}
							} else {
								if (storage->input0.id == 0 || storage->input0.id == en->output0.id) {
									storage->input0.amount += en->output0.amount;
									storage->input0.id = en->output0.id;
									en->output0 = (ItemInstanceData){0};
								}
							}
						}
					}
				}

				// :oxygenerator update
				if (en->arch == ARCH_oxygenerator) {

					if (en->oxygen == 0 && en->input0.id) {
						// consume fuel
						en->input0.amount -= 1;
						if (en->input0.amount <= 0) {
							en->input0 = (ItemInstanceData){0};
						}
						en->oxygen = en->oxygen_max;
					}

					if (en->oxygen > 0) {
						en->frame.is_powered = true;
					}
				}

				// :pickup
				if (is_player_alive() && en->is_item) {

					bool is_exp = en->arch == ARCH_exp;

					float pickup_radius = player_pickup_radius;
					if (is_exp) {
						pickup_radius *= 3;
					}

					bool room_in_inventory = can_add_item_to_inv(en->item);

					if (room_in_inventory && has_reached_end_time(en->pick_up_cooldown_end_time) &&
					fabsf(v2_dist(en->pos, get_player()->pos)) < pickup_radius) {
						en->frame.is_being_picked_up = true;
					}

					if (en->frame.is_being_picked_up) {
						en->has_physics = true;
						en->disable_friction = true;
						Vector2 pick_up_target_pos = player->pos;
						Vector2 target_normal = v2_normalize(v2_sub(pick_up_target_pos, en->pos));

						if (is_exp) {
							// physically accelerate towards the player
							en->acceleration = v2_mulf(target_normal, 2000.0f);
							float mag = v2_length(en->velocity);
							en->velocity = v2_mulf(target_normal, mag);
						} else {
							en->acceleration = v2_mulf(target_normal, 1000.0f);
							float mag = v2_length(en->velocity);
							en->velocity = v2_mulf(target_normal, mag);
						}

						if (v2_dist(en->pos, player->pos) < 2.0f) {

							if (is_exp) {
								play_sound("event:/exp_pickup");
							} else {
								play_sound("event:/item_pickup");
							}

							bool succ = move_item_instance_to_inv(&en->item);
							if (succ) {
								entity_zero_immediately(en);
							}
						}
					} else {
						en->disable_friction = false;
						en->friction = 30;
					}
				}
			}
		}

		// cache all entities that're collidable
		Entity** collision_entities;
		growing_array_init_reserve((void**)&collision_entities, sizeof(Entity*), 1, get_temporary_allocator());
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid && en->has_collision) {
				growing_array_add((void**)&collision_entities, &en);
			}
		}

		// :physics update
		tm_scope("physics")
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (!en->is_valid || !en->has_physics) {
				continue;
			}

			Vector2 next_pos = {0};
			if (en->move_based_on_input_axis) {
				next_pos = v2_add(en->pos, v2_mulf(en->frame.input_axis, en->move_speed * delta_t));
			} else {
				// https://guide.handmadehero.org/code/day043
				
				// "friction"
				if (!en->disable_friction) {
					en->acceleration = v2_sub(en->acceleration, v2_mulf(en->velocity, en->friction));
				}
				// integrate
				en->velocity = v2_add(en->velocity, v2_mulf(en->acceleration, delta_t));
				next_pos = v2_add(en->pos, v2_mulf(en->velocity, delta_t));
				en->acceleration = (Vector2){0};
			}

			if (!en->ignore_collision) {
				Range2f our_bounds = get_entity_collision_bounds(en);
				our_bounds = range2f_shift(our_bounds, en->pos);

				// resolve collisions
				// courtesy of chatgpt
				for (int j = 0; j < growing_array_get_valid_count(collision_entities); j++) {
					Entity* against = collision_entities[j];
					if (against->dim != en->dim) {
						continue;
					}

					// Skip self
					if (against == en) {
						continue;
					}

					// Get the collision bounds of the other entity
					Range2f bounds = range2f_shift(get_entity_collision_bounds(against), against->pos);

					// Get our predicted bounds at next position
					Range2f next_bounds = range2f_shift(get_entity_collision_bounds(en), next_pos);

					// Check for collision between next_bounds and bounds
					bool overlap_x = next_bounds.min.x < bounds.max.x && next_bounds.max.x > bounds.min.x;
					bool overlap_y = next_bounds.min.y < bounds.max.y && next_bounds.max.y > bounds.min.y;

					if (overlap_x && overlap_y) {
						// Collision detected, resolve it

						// Calculate the penetration distances on both axes
						float penetration_x1 = bounds.max.x - next_bounds.min.x; // Positive if overlapping from the left
						float penetration_x2 = next_bounds.max.x - bounds.min.x; // Positive if overlapping from the right
						float penetration_x = (penetration_x1 < penetration_x2) ? penetration_x1 : -penetration_x2;

						float penetration_y1 = bounds.max.y - next_bounds.min.y; // Positive if overlapping from the bottom
						float penetration_y2 = next_bounds.max.y - bounds.min.y; // Positive if overlapping from the top
						float penetration_y = (penetration_y1 < penetration_y2) ? penetration_y1 : -penetration_y2;

						// Resolve collision by moving next_pos out of collision along the axis of least penetration
						if (fabsf(penetration_x) < fabsf(penetration_y)) {
							// Resolve along X axis
							next_pos.x += penetration_x;
							en->velocity.x = 0;
						} else {
							// Resolve along Y axis
							next_pos.y += penetration_y;
							en->velocity.y = 0;
						}
					}
				}
			}

			en->frame.last_pos = en->pos;

			en->pos = next_pos;
		}

		do_portal_teleport_thing(get_player());

		// debug draw collision bounds
		// #fix
		/*
		#if defined(DRAW_BOUNDS)
		{
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid && en->has_collision || en->arch == ARCH_player) {
					Range2f rect = range2f_shift(get_entity_collision_bounds(en), en->pos);
					Draw_Quad* quad = draw_rect_in_frame(rect.min, range2f_size(rect), v4(1, 0, 0, 0.3), current_draw_frame);
					quad->z = 1000;
				}
			}
		}
		#endif
		*/

		/*
		// #fix
		#if defined(MOUSE_COORDS_DEBUG)
		{
			Vector2 mp = get_mouse_pos_in_current_space();
			Tile mp_tile = v2_world_pos_to_tile_pos(mp);
			Vector2i local = world_tile_to_local_map(mp_tile);
			scope_z_layer(10000)
			{
				draw_text(font, tprint("%i, %i", mp_tile.x, mp_tile.y), font_height, mp, v2(0.1, 0.1), COLOR_RED);
				draw_text(font, tprint("%f, %f", mp.x, mp.y), font_height, v2_add(mp, v2(0, -10)), v2(0.1, 0.1), COLOR_RED);
				draw_text(font, tprint("%i, %i", local.x, local.y), font_height, v2_add(mp, v2(0, -20)), v2(0.1, 0.1), COLOR_RED);
			}
		}
		#endif
		*/

		// :player specific caveman update
		{
			Entity* oxygenerator = entity_from_handle(world->oxygenerator);

			Entity* closest_tether = 0;
			float closest_dist = 99999;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* tether = &world->entities[i];
				if (tether->is_valid && tether->is_oxygen_tether && tether->frame.is_powered && tether->arch != ARCH_o2_emitter && tether->dim == get_player_dim()) {
					float dist = v2_dist(tether->pos, player->pos);
					if (dist < tether_connection_radius) {
						if (!closest_tether || dist < closest_dist) {
							closest_tether = tether;
							closest_dist = dist;
						}
					}
				}
			}

			bool connected_to_tether = closest_tether || is_inside;
			app_frame.connected_to_tether = connected_to_tether;

			int start_oxygenerator_o2 = oxygenerator->oxygen;

			// player consumes o2 on tick
			#if !defined(DISABLE_O2)
			{
				float deplete_length_mult = 1.f;
				if (is_inside) {
					deplete_length_mult = 5.f;
				}
				if (player->oxygen_deplete_end_time == 0) {
					player->oxygen_deplete_end_time = now() + oxygen_deplete_tick_length * deplete_length_mult;
				}
				if (has_reached_end_time(player->oxygen_deplete_end_time)) {

					player->oxygen_deplete_end_time = 0;
					player->oxygen -= 1;

					// consume directly from the o2 network
					if (connected_to_tether && oxygenerator->oxygen > 0) {
						player->oxygen += 1;
						oxygenerator->oxygen -= 1;
						assert(oxygenerator->oxygen >= 0);
					}
				}
			}
			#endif

			// o2 -> player pssssssst 
			if (connected_to_tether && oxygenerator->oxygen > 0 && player->oxygen < player->oxygen_max)
			{
				if (player->oxygen_regen_end_time == 0) {
					player->oxygen_regen_end_time = now() + oxygen_regen_tick_length;
				}
				if (has_reached_end_time(player->oxygen_regen_end_time)) {
					player->oxygen_regen_end_time = 0;
					player->oxygen += 1;
					oxygenerator->oxygen -= 1;
				}
			}

			// shutdown sfx when run out
			if (start_oxygenerator_o2 == 1 && oxygenerator->oxygen == 0 && oxygenerator->input0.amount == 0) {
				play_sound_at_pos("event:/shutdown", oxygenerator->pos);
			}

			// player tether line
			// #fix
			/*
			if (connected_to_tether && !is_inside) {
				if (!is_inside) {
					draw_line(v2_add(closest_tether->pos, closest_tether->tether_connection_offset), player->pos, 1.0f, col_tether);
				}
			}
			*/

			player->oxygen = clamp(player->oxygen, 0, player->oxygen_max);

			{
				local_persist FMOD_STUDIO_EVENTINSTANCE* o2_riser = 0;

				if (!connected_to_tether && last_app_frame.connected_to_tether) {
					// just left tether
					play_sound("event:/o2_disconnect");
					#if !defined(DISABLE_O2)
					// o2_riser = play_sound("event:/o2_riser");
					#endif
				}

				if (connected_to_tether && !last_app_frame.connected_to_tether) {
					// just got back to tether
					play_sound("event:/o2_hiss");
					if (o2_riser) {
						stop_sound(o2_riser);
					}
				}
			}

			// TODO, some special LUT desaturation effect as we get closer to oxygen 0
			// :death
			if (is_player_alive() && player->oxygen == 0) {
				player->health = 0;
				// drop inv items
				for (int j = 0; j < ARRAY_COUNT(world->inventory_items); j++) {
					ItemInstanceData* inv_item = &world->inventory_items[j];
					if (inv_item->id == ARCH_exp) {
						*inv_item = empty_item;
					}
					if (inv_item->amount > 0) {
						Entity* drop = entity_create(player->dim);
						setup_item_with_instance(drop, *inv_item);
						drop->pos = player->pos;
					}
					*inv_item = empty_item;
				}

				world->ux_state = UX_respawn;
			}
		}

		// o2 consume on all other entities
		// note, can move this into update now that power is above.
		{
			Entity* o2_genny = entity_from_handle(world->oxygenerator);

			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid && en->o2_consume && en->frame.is_powered) {
					if (en->next_consume_end_time == 0) {
						en->next_consume_end_time = now() + en->o2_consume_rate;
					}

					if (has_reached_end_time(en->next_consume_end_time)) {
						en->next_consume_end_time = 0;
						o2_genny->oxygen -= 1;
					}
				}
			}
		}

		// :selection :interact stuff
		if (is_player_alive())
		{
			Entity* selected_en = world_frame.selected_entity;
			if (selected_en) {
				bool is_in_range = v2_dist(selected_en->pos, get_player()->pos) < player_reach_radius;

				// :click destroy
				if (selected_en->destroyable_world_item) {
					if (is_in_range) {
						selected_en->frame.can_interact = true;
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);

							play_sound_at_pos("event:/hit_generic", selected_en->pos);
							selected_en->white_flash_current_alpha = 1.0;
							camera_shake(0.1);
							particle_emit(selected_en->pos, PFX_hit);

							int damage_amount = 1;
							/*
							if (selected_en->dmg_type == DMG_axe) {
								for (int i = 0; i < ARCH_MAX; i++) {
									if (world->inventory_items[i].amount) {
										damage_amount += get_item_data(i).extra_axe_dmg;
									}
								}
							} else if (selected_en->dmg_type == DMG_pickaxe) {
								for (int i = 0; i < ARCH_MAX; i++) {
									if (world->inventory_items[i].amount) {
										damage_amount += get_item_data(i).extra_pickaxe_dmg;
									}
								}
							} else if (selected_en->dmg_type == DMG_sickle) {
								for (int i = 0; i < ARCH_MAX; i++) {
									if (world->inventory_items[i].amount) {
										damage_amount += get_item_data(i).extra_sickle_dmg;
									}
								}
							} // #extend_dmg_type_here
							*/

							selected_en->health -= damage_amount;

							// old idea, probs too overloaded for us to click on it & mine from it with drills...
							//
							// drop intermittently
							// if (selected_en->big_resource_drop && selected_en->health <= 0) {
							// 	drop_item_at_pos(selected_en->big_resource_drop, selected_en->pos);
							// 	selected_en->health = selected_en->max_health;
							// 	camera_shake(0.1);
							// }

							if (selected_en->health <= 0) {
								camera_shake(0.1); // shake extra on death

								do_entity_drops(selected_en);
								do_entity_exp_drops(selected_en);
								entity_zero_immediately(selected_en);
							}
						}
					} else {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							play_sound("event:/error");
						}
					}

				}

				// generic interact entity
				if (entity_from_handle(world->interacting_with_entity) != selected_en && selected_en->interactable_entity) {
					if (is_in_range) {
						selected_en->frame.can_interact = true;
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							world->ux_state = UX_entity_interaction;
							world->interacting_with_entity = handle_from_entity(selected_en);
						}
					} else {
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							play_sound("event:/error");
						}
					}
				}

				// big resource click error
				if (selected_en->big_resource_drop) {
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						play_sound("event:/error");
					}
				}

				// right click remove
				if (selected_en->right_click_remove) {
					if (is_key_just_pressed(MOUSE_BUTTON_RIGHT)) {
						consume_key_just_pressed(MOUSE_BUTTON_RIGHT);

						if (is_in_range) {
							Entity* drop = entity_create(selected_en->dim);
							setup_item(drop, selected_en->arch);
							drop->pos = selected_en->pos;

							for (int j = 0; j < selected_en->input0.amount; j++) {
								Entity* drop = entity_create(selected_en->dim);
								setup_item(drop, selected_en->input0.id);
								drop->pos = selected_en->pos;
							}
							for (int j = 0; j < selected_en->input1.amount; j++) {
								Entity* drop = entity_create(selected_en->dim);
								setup_item(drop, selected_en->input1.id);
								drop->pos = selected_en->pos;
							}

							entity_zero_immediately(selected_en);
						} else {
							play_sound("event:/error");
						}
					}
				}
			}
		}

		// spawn :world particles
		{
			Vector2 pos = get_player()->pos;

			Range2f rect;
			float scale = 1.5;
			rect.min = v2(-1 * scale, -1 * scale);
			rect.max = v2(1 * scale, 1 * scale);
			rect = m4_transform_range2f(m4_inverse(world_frame.world_proj), rect);
			rect = m4_transform_range2f(world_frame.world_view, rect);
			// {
			// 	draw_rect_in_frame(rect.min, range2f_size(rect), v4(1,0,0,0.8), current_draw_frame);
			// }

			// :glow effect thingo
			{
				float hit_every_seconds = 0.2;
				bool should_hit = false;
				{
					s64 count = app_now() / hit_every_seconds;
					local_persist s64 last_count_triggered = 0;
					if (count > last_count_triggered) {
						should_hit = true;
						last_count_triggered += 1;
					}
				}

				if (is_night() && should_hit) {
					Vector4 col = hex_to_rgba(0xe7c756ff);

					Particle* p = particle_new();
					p->flags |= PARTICLE_FLAGS_physics | PARTICLE_FLAGS_light;
					p->pos.x = get_random_float32_in_range(rect.min.x, rect.max.x);
					p->pos.y = get_random_float32_in_range(rect.min.y, rect.max.y);
					p->velocity.x = get_random_float32_in_range(-4, 4);
					p->velocity.y = 10;
					p->col = col;
					p->light_col = col;
					p->light_col.a = 1.0; // drives effect of point light
					p->light_intensity = 0.2;
					p->light_radius = 10;
					p->fade_in_pct = 0.1;
					p->fade_out_pct = 0.2;
					p->lifetime_length = 5;

					Particle* p2 = particle_new();
					*p2 = *p;
					p2->light_radius = 30;
					p2->light_col = v4(0,0,0,0);
					p2->light_intensity = 0.5;
				}
			}

			// daytime ambiance particles 
			{
				float hit_every_seconds = 0.2;
				bool should_hit = false;
				{
					s64 count = app_now() / hit_every_seconds;
					local_persist s64 last_count_triggered = 0;
					if (count > last_count_triggered) {
						should_hit = true;
						last_count_triggered += 1;
					}
				}

				if (should_hit) {
					Vector4 col = hex_to_rgba(0x797979ff);

					Particle* p = particle_new();
					p->flags |= PARTICLE_FLAGS_physics;
					p->pos.x = get_random_float32_in_range(rect.min.x, rect.max.x);
					p->pos.y = get_random_float32_in_range(rect.min.y, rect.max.y);
					p->velocity.x = get_random_float32_in_range(-2, 2);
					p->velocity.y = get_random_float32_in_range(5, 6);
					p->col = col;
					p->fade_in_pct = 0.1;
					p->fade_out_pct = 0.2;
					p->lifetime_length = 5;
				}
			}

		}

		particle_update();		

		// day/night :cycle
		{
			float day_length = 10 * 60;
			float night_length = 3 * 60;
			float transition_length = 10.0f;

			if (world->cycle_end_time == 0) {
				world->cycle_end_time = now() + day_length; // init to start of day
			}

			if (has_reached_end_time(world->cycle_end_time)) {
				bool is_day = world->night_alpha_target == 0;

				if (is_day) {
					world->night_alpha_target = 1.0f;
					world->cycle_end_time = now() + night_length + transition_length;
					log("transition to night");
				} else {
					world->night_alpha_target = 0.0f;
					world->cycle_end_time = now() + day_length + transition_length;
					world->day_count += 1;
					log("transition to day");
				}
			}

			animate_f32_to_target_linear_with_epsilon(&world->night_alpha, world->night_alpha_target, delta_t, 1.0/transition_length, 0.001);
		}

		// :rendering pipeline
		{
			assert(growing_array_get_valid_count(draw_frame.quad_buffer) == 0, "submitting quads prior to the rendering pass is a no go");

			// :portal rendering
			Gfx_Image** target_portals;
			growing_array_init_reserve((void**)&target_portals, sizeof(Gfx_Image*), 1, get_temporary_allocator());

			for (int i = 0; i < MAX_ENTITY_COUNT; i++)
			tm_scope("portal render")
			{
				Entity* portal = &world->entities[i];
				if (!(is_valid(portal) && portal->arch == ARCH_portal && portal->render_target_image)) continue;

				// TODO use frame's item to turn portal on / off

				Gfx_Image* target_image = portal->render_target_image;

				current_draw_frame = &offscreen_draw_frame;
				draw_frame_reset(&offscreen_draw_frame);
				gfx_clear_render_target(target_image, COLOR_BLACK);

				WorldFrame prev_world_frame = world_frame;

				// setup the camera
				{
					Vector2 frame_size = get_sprite_size(get_sprite(SPRITE_portal_frame));

					Vector2 dest_pos = portal->portal_view_pos;

					Vector2 portal_offset = get_offset_for_rendering(portal->arch);

					// worldspace view rect
					Range2f rect;
					rect.min = v2_add(dest_pos, portal_offset);
					rect.max = v2_add(rect.min, frame_size);

					// Compute the center and size of the rectangle
					Vector2 rect_center = v2_mulf(v2_add(rect.min, rect.max), 0.5f);
					Vector2 rect_size = v2_sub(rect.max, rect.min);

					// Calculate scaling factors based on the window size
					float scale_x = window.width / rect_size.x;
					float scale_y = window.height / rect_size.y;

					// Create scaling and translation matrices
					Matrix4 S = m4_make_scale(v3(scale_x, scale_y, 1.0f));
					Matrix4 T = m4_make_translation(v3(-rect_center.x, -rect_center.y, 0.0f));

					// Combine to form the view matrix
					Matrix4 view = m4_mul(S, T);

					world_frame.camera_pos_copy = dest_pos;
					world_frame.world_view = m4_inverse(view);

					world_frame.render_target_w = target_image->width;
					world_frame.render_target_h = target_image->height;
				}

				cbuffer = (ShaderConstBuffer){0};
				draw_world_in_frame(portal->dimension_target);

				offscreen_draw_frame.enable_z_sorting = true;
				offscreen_draw_frame.shader_extension = global_shader;
				offscreen_draw_frame.cbuffer = &cbuffer;

				gfx_render_draw_frame(&offscreen_draw_frame, target_image);
				current_draw_frame = 0;

				world_frame = prev_world_frame;

				growing_array_add((void**)&target_portals, &target_image);
			}

			{
				draw_frame_reset(&offscreen_draw_frame);
				gfx_clear_render_target(game_image, COLOR_BLACK);

				cbuffer = (ShaderConstBuffer){0};
				{
					current_draw_frame = &offscreen_draw_frame;

					world_frame.draw_portals = true;
					for (int i = 0; i < growing_array_get_valid_count(target_portals); i++) {
						Gfx_Image* img = target_portals[i];
						// todo, make this actually work with multiple lol
						draw_frame_bind_image_to_shader(&offscreen_draw_frame, img, 0);
					}

					draw_world_in_frame(get_player_dim());
					current_draw_frame = 0;
				}

				offscreen_draw_frame.enable_z_sorting = true;
				offscreen_draw_frame.shader_extension = global_shader;
				offscreen_draw_frame.cbuffer = &cbuffer;

				gfx_render_draw_frame(&offscreen_draw_frame, game_image);
			}

			{
				gfx_render_draw_frame(&ui_draw_frame, ui_image);
			}

			Draw_Quad *q = draw_image(game_image, v2(-window.width/2, -window.height/2), v2(window.width, window.height), COLOR_WHITE);
			swap(q->uv.y, q->uv.w, float); // swap y so it's upwards

			q = draw_image(ui_image, v2(-window.width/2, -window.height/2), v2(window.width, window.height), COLOR_WHITE);
			swap(q->uv.y, q->uv.w, float); // swap y so it's upwards
		}


		tm_scope("gfx_update") {
			gfx_update();
		}
		tm_scope("fmod update") {
			fmod_update();
		}
		seconds_counter += delta_t;
		frame_count += 1;
		if (seconds_counter > 1.0) {
			#if ENABLE_PROFILING
			log("fps: %i", frame_count);
			#endif
			seconds_counter = 0.0;
			frame_count = 0;
		}

		if (is_key_just_pressed('P')) {
			dump_profile_result();
		}

		// load/save commands
		// these are at the bottom, because we'll want to have a clean spot to do this to avoid any mid-way operation bugs.
		#if CONFIGURATION == DEBUG
		if (is_key_down(KEY_ALT))
		{
			if (is_key_just_pressed('X')) {
				shader_recompile();
				log("reloaded shader");
			}
			if (is_key_just_pressed('N')) {
				// instantly cycle to next day/night
				world->cycle_end_time = now();
			}
			if (is_key_just_pressed('F')) {
				world_save_to_disk();
				log("saved");
			}
			if (is_key_just_pressed('R')) {
				world_attempt_load_from_disk();
				log("loaded ");
			}
		}
		if (is_key_just_pressed('K') && is_key_down(KEY_SHIFT)) {
			memset(world, 0, sizeof(World));
			memset(&world_frame, 0, sizeof(WorldFrame));
			world_setup();
			log("reset");
		}
		#endif
	}

	world_save_to_disk();
	fmod_shutdown();

	return 0;
}