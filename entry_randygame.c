// The entry function is at the bottom of this file.
// Go there if you want to read this codebase.
// C needs declarations to be ordered, and header files are a waste of time. So reading this top to bottom isn't recommended.

#include "range.c"
#include "config.c"
#include "easings.c"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

// got this from ryan fleury's codebase, https://www.rfleury.com/ (originally called DeferLoop)
#define defer_scope(start, end) for(int _i_ = ((start), 0); _i_ == 0; _i_ += 1, (end))

#define scope_z_layer(Z) defer_scope(push_z_layer(Z), pop_z_layer())

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

Gfx_Text_Metrics draw_text_with_pivot(Gfx_Font *font, string text, u32 raster_height, Vector2 position, Vector2 scale, Vector4 color, Pivot pivot) {
	Gfx_Text_Metrics metrics = measure_text(font, text, raster_height, scale);
	position = v2_sub(position, metrics.visual_pos_min);
	Vector2 pivot_mul = {0};
	switch (pivot) {
		case PIVOT_bottom_left: pivot_mul = v2(0.0, 0.0); break;
		case PIVOT_bottom_center: pivot_mul = v2(0.5, 0.0); break;
		case PIVOT_bottom_right: pivot_mul = v2(1.0, 0.0); break;
		case PIVOT_center_center: pivot_mul = v2(0.5, 0.5); break;
		case PIVOT_center_left: pivot_mul = v2(0.0, 0.5); break;
		case PIVOT_top_center: pivot_mul = v2(0.5, 1.0); break;
		case PIVOT_top_left: pivot_mul = v2(0.0, 1.0); break;
		default:
		log_error("pivot not supported yet. fill in case at draw_text_with_pivot");
		break;
	}
	position = v2_sub(position, v2_mul(metrics.visual_size, pivot_mul));
	draw_text(font, text, raster_height, position, scale, color);
	return metrics;
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

Draw_Quad ndc_quad_to_screen_quad(Draw_Quad ndc_quad) {

	// NOTE: we're assuming these are the screen space matricies.
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;

	Matrix4 ndc_to_screen_space = m4_identity;
	ndc_to_screen_space = m4_mul(ndc_to_screen_space, m4_inverse(proj));
	ndc_to_screen_space = m4_mul(ndc_to_screen_space, view);

	ndc_quad.bottom_left = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.bottom_left), 0, 1)).xy;
	ndc_quad.bottom_right = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.bottom_right), 0, 1)).xy;
	ndc_quad.top_left = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.top_left), 0, 1)).xy;
	ndc_quad.top_right = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.top_right), 0, 1)).xy;

	return ndc_quad;
}

// :utils

bool get_random_bool() {
	return get_random_int_in_range(0, 1);
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

Range2f quad_to_range(Draw_Quad quad) {
	return (Range2f){quad.bottom_left, quad.top_right};
}

// ^^^ generic utils

// :col
// these get inited on startup because we don't have a #run for the hex_to_rgba(0x2a2d3aff) lol
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
float selection_reach_radius = 30.0f;
float tether_connection_radius = 50.0;
float o2_full_tank_deplete_length = 16.0f; // #volatile length of oxygen riser sfx
float oxygen_regen_tick_length = 0.01;
float oxygen_deplete_tick_length = 1.f;
float teleporter_radius = 8.0f;
const int tile_width = 8;
const float world_half_length = tile_width * 10;
const float entity_selection_radius = 16.0f;
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
	layer_background = 9,
	layer_world = 10,
	layer_buildings,
	layer_ui = 20,
	layer_tooltip,
	layer_cursor_item,
} Layers;

// global :app stuff
// we could move this into an AppState struct.
// or we could just keep cavemaning it like this lol, since it's not needed.
float exp_error_flash_alpha = 0;
float exp_error_flash_alpha_target = 0;
float camera_trauma = 0;
float zoom = 5.3;
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

// :shader
typedef struct ShaderConstBuffer {
	int we_dont_have_a_use_for_this_yet;
} ShaderConstBuffer;
ShaderConstBuffer cbuffer = {0};

// #volatile
void set_col_override(Draw_Quad* q, Vector4 col_override) {
	q->userdata[0] = col_override;
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
	SPRITE_teleporter1,
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

// #refactor_idea_that_is_purely_aesthetic
// could we merge itemid into archid? and just use a flag to represent it?
// I remember trying this a while back. I got frustated with it being too overloaded I think.
//
// I guess probably not, because there's a distinction from an item on the ground, and the placed big chungus entity?
//
typedef enum ItemID {
	ITEM_nil,
	ITEM_rock,
	ITEM_pine_wood,
	ITEM_exp,
	ITEM_raw_copper,
	ITEM_copper_ingot,
	ITEM_fiber,
	ITEM_flint,
	ITEM_flint_axe,
	ITEM_flint_pickaxe,
	ITEM_flint_scythe,
	ITEM_coal,
	ITEM_o2_shard,
	ITEM_raw_iron,
	ITEM_iron_ingot,
	ITEM_furnace,
	ITEM_workbench,
	ITEM_research_station,
	ITEM_teleporter1,
	ITEM_tether,
	ITEM_burner_drill,
	ITEM_longboi_test,
	ITEM_wall,
	ITEM_wall_gate,
	ITEM_o2_emitter,
	// :item
	ITEM_MAX,
} ItemID;

typedef struct InventoryItemData {
	int amount;
} InventoryItemData; 

typedef struct ItemAmount {
	ItemID id;
	int amount;
} ItemAmount; 

typedef enum ArchetypeID {
	ARCH_nil = 0,
	ARCH_rock = 1,
	ARCH_tree = 2,
	ARCH_player = 3,
	ARCH_item = 4,
	// 5
	ARCH_furnace = 6,
	ARCH_workbench = 7,
	ARCH_research_station = 8,
	ARCH_exp_vein = 9,
	ARCH_teleporter1 = 10,
	ARCH_copper_depo = 11,
	ARCH_flint_depo = 12,
	ARCH_grass = 13,
	ARCH_oxygenerator = 14,
	ARCH_tether = 15,
	ARCH_exp_orb = 16,
	ARCH_ice_vein = 17,
	ARCH_tile_resource = 18,
	ARCH_burner_drill = 19,
	ARCH_longboi_test = 20,
	ARCH_coal_depo = 21,
	ARCH_iron_depo = 22,
	ARCH_wall = 23,
	ARCH_wall_gate = 24,
	ARCH_o2_emitter = 25,
	// :arch
	ARCH_MAX,
} ArchetypeID;

// :item
typedef struct ItemData {
	string pretty_name;
	string description;
	SpriteID icon;
	int extra_axe_dmg; // #extend_dmg_type_here
	int extra_pickaxe_dmg;
	int extra_sickle_dmg;
	ArchetypeID for_structure;
	float craft_length;
	ItemID furnace_transform_into;
	bool disabled;
	// merged in from building.
	ArchetypeID to_build;
	int exp_cost;
	ItemAmount ingredients[8];
	int ingredients_count;

} ItemData;
ItemData item_data[ITEM_MAX] = {0};
ItemData get_item_data(ItemID id) {
	return item_data[id];
}
SpriteID get_sprite_id_from_item(ItemID id) {
	return get_item_data(id).icon;
}

typedef enum DamageType {
	DMG_nil,
	DMG_axe,
	DMG_pickaxe,
	DMG_sickle,
	// #extend_dmg_type_here
	DMG_MAX,
} DamageType;

typedef struct Entity Entity; // needs forward declare
typedef struct EntityFrame {
	Entity** connected_to_tethers;
	bool is_powered;
	Vector2 input_axis;
	SpriteID functional_sprite_id;
	// :frame
} EntityFrame;

typedef struct Entity {
	bool is_valid;
	ArchetypeID arch;
	ItemID item_id;
	int item_amount;
	Vector2 pos;
	bool render_sprite;
	SpriteID sprite_id;
	int health;
	int max_health;
	bool destroyable_world_item;
	int current_crafting_amount;
	float64 crafting_end_time;
	ItemAmount drops[4];
	int drops_count;
	DamageType dmg_type;
	ItemID selected_crafting_item;
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
	bool is_being_picked_up;
	bool disable_friction;
	float64 pick_up_cooldown_end_time;
	float white_flash_current_alpha;
	int exp_amount;
	ItemAmount input0;
	ItemAmount input1;
	int anim_index;
	float64 time_til_next_frame;
	Vector2i last_move_dir;
	float64 next_hit_end_time;
	float radius;
	bool interactable_entity;
	int last_fuel_max;
	int current_fuel;
	bool offset_based_on_tile_height;
	ItemID current_crafting_item;
	int progress_on_crafting; // this goes up to 100
	float64 next_crafting_progress_tick_end_time;
	bool has_collision;
	Range2f collision_bounds;
	bool ignore_collision;
	bool wall_seal;

	// state that is completely constant, derived by archetype
	// this was originally in a separate data structure, but it feels better inside here.
	// even though it's a "waste of memory" since these are constant across all entities.
	Vector2i tile_size;
	string pretty_name;
	//

	EntityFrame frame;
	EntityFrame last_frame;
	// :entity
} Entity;
#define MAX_ENTITY_COUNT 1024

typedef enum UXState {
	UX_nil,
	UX_inventory,
	UX_place_mode,
	UX_workbench,
	UX_research,
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

typedef struct TileData {
	Tile tile;
	Entity* entity_at_tile;
} TileData;

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
	// :biome #volatile
	BIOME_MAX,
} BiomeID;

typedef struct WorldResourceData {
	BiomeID biome_id;
	ArchetypeID arch_id;
	int dist_from_self;
} WorldResourceData;
// NOTE - trying out a new pattern here. That way we don't have to keep writing up enums to index into these guys. If we need dynamic runtime data, just make an array with the count of this array and have it essentially share the index. Like what I've done below in the world state.
WorldResourceData world_resources[] = {
	{ BIOME_forest, ARCH_tree, 5 },
	{ BIOME_forest, ARCH_grass, 3 },

	{ BIOME_barren, ARCH_rock, 10 },
	{ BIOME_barren, ARCH_coal_depo, 20 },

	{ BIOME_copper, ARCH_copper_depo, 10 },

	{ BIOME_copper_heavy, ARCH_copper_depo, 4 },
	{ BIOME_copper_heavy, ARCH_flint_depo, 20 },
	{ BIOME_copper_heavy, ARCH_rock, 10 },

	{ BIOME_ice, ARCH_ice_vein, 10 },

	{ BIOME_iron, ARCH_iron_depo, 10 },
	// :spawn_res system
};

typedef struct Map {
	int width;
	int height;
	BiomeID* tiles;
} Map;
Map map = {0};

u32 biome_colors[BIOME_MAX] = {
	0x000000, // void
	0xffffff, // core,
	0x484848, // barren,
	0x3553b9, // forest,
	0xbf6937, // copper,
	0xa54b18, // copper heavy
	0x70d6cd, // ice
	0x26c9bc, // ice heavy
	0xe49f9f, // iron
	// :biome #volatile
};

Vector4 biome_col_hex_to_rgba(u32 col) {
	u8 r = (col>>16) & 0x000000FF;
	u8 g = (col>>8) & 0x000000FF;
	u8 b = (col>>0) & 0x000000FF;
	return (Vector4){r/255.0, g/255.0, b/255.0, 1};
}

void init_biome_maps() {

	string png;
	bool ok = os_read_entire_file("res/sprites/biome0_map.png", &png, get_heap_allocator());
	assert(ok);

	int width, height, channels;
	stbi_set_flip_vertically_on_load(1);
	third_party_allocator = get_heap_allocator();
	u8* stb_data = stbi_load_from_memory(png.data, png.count, &width, &height, &channels, STBI_rgb_alpha);
	assert(stb_data);
	assert(channels == 4);
	third_party_allocator = ZERO(Allocator);

	map.width = width;
	map.height = height;
	map.tiles = alloc(get_heap_allocator(), width * height * sizeof(BiomeID));

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
				map.tiles[index] = i;
				found_biomes[i] = true;
			}
		}
	}

	for (BiomeID i = 0; i < BIOME_MAX; i++) {
		if (!found_biomes[i]) {
			log_warning("Biome %i is unused", i);
		}
	}
}

Tile local_map_to_world_tile(Vector2i local) {
	return (Tile) {
		local.x - floor((float)map.width * 0.5),
		(local.y-map.height) + floor((float)(map.height) * 0.5) + 1,
	};
}

Vector2i world_tile_to_local_map(Tile world) {
	int x_index = world.x + floor((float)map.width * 0.5);
	int y_index = world.y + floor((float)map.height * 0.5);
	x_index = clamp(x_index, 0, map.width-1);
	y_index = clamp(y_index, 0, map.height-1);
	return (Vector2i){x_index, y_index};
}

int local_map_pos_to_index(Vector2i local_map) {
	int index = local_map.y * map.width + local_map.x;
	return index;
}

BiomeID biome_at_tile(Tile tile) {
	BiomeID biome = 0;
	int x_index = tile.x + floor((float)map.width * 0.5);
	int y_index = tile.y + floor((float)map.height * 0.5);
	if (x_index < map.width && x_index >= 0 && y_index < map.height && y_index >= 0) {
		biome = map.tiles[y_index * map.width + x_index];
	}
	return biome;
}

typedef struct World {
	float64 time_elapsed;
	Entity entities[MAX_ENTITY_COUNT];
	InventoryItemData inventory_items[ITEM_MAX];
	UXState ux_state;
	float inventory_alpha;
	float inventory_alpha_target;
	float building_alpha;
	float building_alpha_target;
	ItemID placing_building;
	Entity* interacting_with_entity;
	ItemID selected_research_thing;
	UnlockState item_unlocks[ITEM_MAX];
	float64 resource_next_spawn_end_time[ARRAY_COUNT(world_resources)];
	// todo #ship - figure out if we can legit just keep this as a pointer or not lol
	Entity* oxygenerator;
	ItemAmount mouse_cursor_item;
	// :world :state
} World;
World* world = 0;

typedef struct WorldFrame {
	Entity* selected_entity;
	Matrix4 world_proj;
	Matrix4 world_view;
	bool hover_consumed;
	bool show_inventory;
	Entity* player;
	// :frame state
} WorldFrame;
WorldFrame world_frame;

Entity* get_player() {
	return world_frame.player;
}

// I <3 C
#include "fmod_sound.c"

bool is_player_alive() {
	return get_player()->health > 0;
}

void entity_apply_defaults(Entity* en) {
	en->tile_size = v2i(1, 1);
}

Entity* entity_create() {
	Entity* entity_found = 0;
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* existing_entity = &world->entities[i];
		if (!existing_entity->is_valid) {
			entity_found = existing_entity;
			break;
		}
	}
	assert(entity_found, "No more free entities!");
	entity_found->is_valid = true;
	entity_apply_defaults(entity_found);

	return entity_found;
}

void entity_destroy(Entity* entity) {
	memset(entity, 0, sizeof(Entity));
}

// :arch :setup things

void setup_o2_emitter(Entity* en) {
	en->arch = ARCH_o2_emitter;
	en->pretty_name = STR("Oxygen Feeder");
	en->is_oxygen_tether = true;
	en->tile_size = v2i(1, 1);
	en->sprite_id = SPRITE_o2_emitter;
	en->has_collision = true;
	en->collision_bounds = range2f_make_center_center(v2(0, 0), v2(tile_width, tile_width));
	en->wall_seal = true;
}

void setup_wall(Entity* en) {
	en->arch = ARCH_wall;
	en->pretty_name = STR("Wall");
	en->tile_size = v2i(1, 1);
	en->sprite_id = SPRITE_wall;
	en->has_collision = true;
	en->collision_bounds = range2f_make_center_center(v2(0, 0), v2(tile_width, tile_width));
	en->wall_seal = true;
}
void setup_wall_gate(Entity* en) {
	en->arch = ARCH_wall_gate;
	en->pretty_name = STR("Wall Gate");
	en->tile_size = v2i(1, 1);
	en->sprite_id = SPRITE_wall_gate;
	en->wall_seal = true;
}

void setup_iron_depo(Entity* en) {
	en->arch = ARCH_iron_depo;
	en->tile_size = v2i(2, 1);
	en->sprite_id = SPRITE_iron_depo;
	en->health = 10;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_raw_iron, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_coal_depo(Entity* en) {
	en->arch = ARCH_coal_depo;
	en->tile_size = v2i(2, 1);
	en->sprite_id = SPRITE_coal_depo;
	en->health = 10;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_coal, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_longboi_test(Entity* en) {
	en->arch = ARCH_longboi_test;
	en->pretty_name = STR("asdf");
	en->tile_size = v2i(3, 1);
	en->sprite_id = SPRITE_longboi_test;
}

void setup_burner_drill(Entity* en) {
	en->arch = ARCH_burner_drill;
	en->pretty_name = STR("Thumper");
	en->tile_size = v2i(2, 2);
	en->sprite_id = SPRITE_burner_drill;
	en->radius = tile_width * 4;
	en->interactable_entity = true;
	en->has_collision = true;
}

void setup_tile_resource(Entity* en) {
	en->arch = ARCH_tile_resource;
	en->sprite_id = SPRITE_ice_tile;
}

void setup_ice_vein(Entity* en) {
	en->arch = ARCH_ice_vein;
	en->sprite_id = SPRITE_ice_vein;
	en->health = ice_vein_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_o2_shard, .amount=1};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_exp_orb(Entity* en) {
	en->arch = ARCH_exp_orb;
	en->exp_amount = 1;
}

void setup_tether(Entity* en) {
	en->arch = ARCH_tether;
	en->pretty_name = STR("Tether");
	en->sprite_id = SPRITE_tether;
	en->is_oxygen_tether = true;
	en->tether_connection_offset.y = 4;
}

void setup_oxygenerator(Entity* en) {
	en->arch = ARCH_oxygenerator;
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
	en->arch = ARCH_grass;
	en->sprite_id = SPRITE_grass;
	en->health = grass_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_fiber, .amount=get_random_int_in_range(1, 2)};
	en->dmg_type = DMG_sickle;
}

void setup_flint_depo(Entity* en) {
	en->arch = ARCH_flint_depo;
	en->tile_size = v2i(2, 1);
	en->sprite_id = SPRITE_flint_depo;
	en->health = flint_depo_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_flint, .amount=get_random_int_in_range(1, 2)};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_copper_depo(Entity* en) {
	en->arch = ARCH_copper_depo;
	en->sprite_id = SPRITE_copper_depo;
	en->health = copper_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_raw_copper, .amount=get_random_int_in_range(1, 2)};
	en->dmg_type = DMG_pickaxe;
	en->has_collision = true;
}

void setup_teleporter1(Entity* en) {
	en->arch = ARCH_teleporter1;
	en->pretty_name = STR("Teleporter");
	en->sprite_id = SPRITE_teleporter1;
}

void setup_exp_vein(Entity* en) {
	en->arch = ARCH_exp_vein;
	en->sprite_id = SPRITE_exp_vein;
	en->health = exp_vein_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_exp, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_pickaxe;
}

void setup_furnace(Entity* en) {
	en->arch = ARCH_furnace;
	en->tile_size = v2i(2, 2);
	en->pretty_name = STR("Furnace");
	en->sprite_id = SPRITE_furnace;
	en->interactable_entity = true;
	en->has_collision = true;
}

void setup_workbench(Entity* en) {
	en->arch = ARCH_workbench;
	en->pretty_name = STR("Fabricator");
	en->sprite_id = SPRITE_fabricator;
	en->interactable_entity = true;
	en->tile_size = v2i(2,2);
	en->has_collision = true;
}

void setup_research_station(Entity* en) {
	en->arch = ARCH_research_station;
	en->tile_size = v2i(2, 1);
	en->pretty_name = STR("Research Station");
	en->sprite_id = SPRITE_research_station;
	en->interactable_entity = true;
	en->has_collision = true;
}

void setup_player(Entity* en) {
	en->arch = ARCH_player;
	en->health = 1;
	en->max_health = en->health;
	en->oxygen_max = 17;
	en->oxygen = en->oxygen_max;
	en->has_physics = true;
	en->friction = 20.f;
	en->collision_bounds = range2f_make_center_center(v2(0,0), v2(6, 9));
}

void setup_rock(Entity* en) {
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock0;
	en->health = rock_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_rock, .amount=1};
	en->dmg_type = DMG_pickaxe;
}

void setup_tree(Entity* en) {
	en->arch = ARCH_tree;
	en->tile_size = v2i(2, 2);
	en->offset_based_on_tile_height = true;
	en->sprite_id = SPRITE_tree1;
	// en->sprite_id = SPRITE_tree1;
	en->health = tree_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_pine_wood, .amount=1};
	en->dmg_type = DMG_axe;
	en->has_collision = true;
	en->collision_bounds = range2f_make_bottom_center(v2(4, 4));
	en->collision_bounds = range2f_shift(en->collision_bounds, v2(0, -tile_width));
}

void setup_item(Entity* en, ItemID item_id) {
	en->arch = ARCH_item;
	en->sprite_id = get_sprite_id_from_item(item_id);
	en->item_id = item_id;
	en->item_amount = 1;
	en->isnt_a_tile = true;
	en->ignore_collision = true;
}

void entity_setup(Entity* en, ArchetypeID id) {
	switch (id) {
		// filling these in so we can get compiler errors for missing cases
		case ARCH_nil: break;
		case ARCH_MAX: break;
		case ARCH_item: setup_item(en, 0); log_warning("setting up item in entity_setup ???"); break;
		//
		case ARCH_exp_orb: setup_exp_orb(en); break;
		case ARCH_player: setup_player(en); break;
		case ARCH_furnace: setup_furnace(en); break;
		case ARCH_workbench: setup_workbench(en); break;
		case ARCH_research_station: setup_research_station(en); break;
		case ARCH_rock: setup_rock(en); break;
		case ARCH_tree: setup_tree(en); break;
		case ARCH_exp_vein: setup_exp_vein(en); break;
		case ARCH_teleporter1: setup_teleporter1(en); break;
		case ARCH_copper_depo: setup_copper_depo(en); break;
		case ARCH_flint_depo: setup_flint_depo(en); break;
		case ARCH_grass: setup_grass(en); break;
		case ARCH_oxygenerator: setup_oxygenerator(en); break;
		case ARCH_tether: setup_tether(en); break;
		case ARCH_ice_vein: setup_ice_vein(en); break;
		case ARCH_tile_resource: setup_tile_resource(en); break;
		case ARCH_burner_drill: setup_burner_drill(en); break;
		case ARCH_longboi_test: setup_longboi_test(en); break;
		case ARCH_coal_depo: setup_coal_depo(en); break;
		case ARCH_iron_depo: setup_iron_depo(en); break;
		case ARCH_wall: setup_wall(en); break;
		case ARCH_wall_gate: setup_wall_gate(en); break;
		case ARCH_o2_emitter: setup_o2_emitter(en); break;
		// :arch setup
	}
}

Entity entity_archetype_data[ARCH_MAX] = {0};
void setup_entity_archetype_data_cache() {
	for (ArchetypeID i = 1; i < ARCH_MAX; i++) {
		Entity* en = &entity_archetype_data[i];
		entity_apply_defaults(en);
		entity_setup(en, i);
	}
}
Entity get_archetype_data(ArchetypeID id) {
	return entity_archetype_data[id];
}
string get_archetype_pretty_name(ArchetypeID id) {
	return get_archetype_data(id).pretty_name;
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

Vector2 get_mouse_pos_in_world_space() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
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
void set_screen_space() {
	draw_frame.camera_xform = m4_scalar(1.0);
	draw_frame.projection = m4_make_orthographic_projection(0.0, screen_width, 0.0, screen_height, -1, 10);
}
void set_world_space() {
	draw_frame.projection = world_frame.world_proj;
	draw_frame.camera_xform = world_frame.world_view;
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

	return draw_image(sprite->image, rect.min, range2f_size(rect), col);
}

// pad_pct just shrinks the rect by a % of itself ... 0.2 is a nice default
Draw_Quad* draw_sprite_in_rect(SpriteID sprite_id, Range2f rect, Vector4 col, float pad_pct) {
	return draw_sprite_in_rect_with_xform(sprite_id, rect, col, pad_pct, m4_identity);
}

inline float64 now() {
	return world->time_elapsed;
}

float alpha_from_end_time(float64 end_time, float length) {
	return float_alpha(now(), end_time-length, end_time);
}

bool has_reached_end_time(float64 end_time) {
	return now() > end_time;
}

// :func dump

Range2f get_entity_collision_bounds(Entity* en) {
	Range2f bounds = en->collision_bounds;
	if (almost_equals(range2f_size(bounds).x, 0, 0.1) || almost_equals(range2f_size(bounds).y, 0, 0.1)) {
		// auto-gen bounds from sprite size
		bounds = range2f_make_center_center(v2(0,0), get_sprite_size(get_sprite(en->sprite_id)));
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

Vector2 get_offset_for_rendering(Entity* en) {
	Sprite* sprite = get_sprite(en->sprite_id);

	Vector2 offset = {0};
	offset.x -= get_sprite_size(sprite).x * 0.5;

	if (en->offset_based_on_tile_height) {
		offset.y -= en->tile_size.y * tile_width * 0.5;
	} else {
		offset.y -= get_sprite_size(sprite).y * 0.5;
	}

	return offset;
}

Range2f get_entity_range(Entity* en) {
	Vector2 bottom_left = v2_add(en->pos, get_offset_for_rendering(en));
	return (Range2f){ bottom_left, v2_add(bottom_left, get_sprite_size(get_sprite(en->sprite_id))) };
}

void do_entity_drops(Entity* en) {
	ItemID *drops;
	// purposefully making the reserve 2 items, to prove the resizing works, and that you don't have to worry about the size of the array.
	growing_array_init_reserve((void**)&drops, sizeof(ItemID), 2, get_temporary_allocator());

	// drops from entity data
	for (int i = 0; i < en->drops_count; i++) {
		ItemAmount drop = en->drops[i];
		for (int j = 0; j < drop.amount; j++) {
			growing_array_add((void**)&drops, &drop.id);
		}
	}

	// create all the item drops
	for (int i = 0; i < growing_array_get_valid_count(drops); i++) {
		ItemID drop_id = drops[i];
		Entity* drop = entity_create();
		setup_item(drop, drop_id);
		drop->pos = en->pos;
		drop->pos = v2_add(drop->pos, v2(get_random_float32_in_range(-2, 2), get_random_float32_in_range(-2, 2)));
		drop->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.1, 0.3);
	}
}

Vector2 v2_tile_pos_to_entity_world_pos(Vector2i tile_pos, ArchetypeID id) {
	Vector2 pos = v2_tile_pos_to_world_pos(tile_pos);
	pos.x += get_archetype_data(id).tile_size.x * tile_width * 0.5;
	pos.y += get_archetype_data(id).tile_size.y * tile_width * 0.5;
	return pos;
}

void draw_item_amount_in_rect(ItemAmount item_amount, Range2f rect) {
	draw_sprite_in_rect(get_sprite_id_from_item(item_amount.id), rect, COLOR_WHITE, 0.1);

	float x0 = rect.max.x;
	float y0 = rect.min.y;
	x0 -= 1;
	y0 += 1;

	Vector2 text_scale = v2(0.1, 0.1);
	string txt = tprint("%i", item_amount.amount);
	draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_bottom_right);
}

void item_tooltip(ItemAmount item_amount) {
	push_z_layer(layer_tooltip);

	Vector2 text_scale = v2(0.1, 0.1);
	ItemData item_data = get_item_data(item_amount.id);

	Vector2 size = v2(40, 20);
	Vector2 pos = get_mouse_pos_in_world_space();

	draw_rect(pos, size, COLOR_BLACK);

	float x0, y0;
	x0 = pos.x + size.x * 0.5;
	y0 = pos.y + size.y;
	y0 -= 2.f; // arbitrary padding

	Gfx_Text_Metrics met = draw_text_with_pivot(font, item_data.pretty_name, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);

	pop_z_layer();
}

void camera_shake(float amount) {
	camera_trauma += amount;
}

typedef struct TileCache {
	Entity* entity;
	// BiomeID biome;
	bool visited;
} TileCache;
typedef struct TileEntityCache {
	TileCache* tiles;
	int tile_count;
} TileEntityCache;
TileEntityCache* create_tile_entity_pair_cache() {
	TileEntityCache* cache = alloc(get_temporary_allocator(), sizeof(TileEntityCache));

	cache->tile_count = map.height * map.width;
	cache->tiles = alloc(get_temporary_allocator(), sizeof(TileCache) * cache->tile_count);

	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* en = &world->entities[i];
		Vector2i local_tile_pos = world_tile_to_local_map(v2_world_pos_to_tile_pos(en->pos));
		int index = local_tile_pos.y * map.width + local_tile_pos.x;
		if (index >= 0 && index < cache->tile_count) {
			cache->tiles[index].entity = en;
		}
	}

	return cache;
}

TileCache* tile_cache_at_tile(TileEntityCache* cache, Tile tile) {
	Vector2i local_tile = world_tile_to_local_map(tile);
	int index = local_tile.y * map.width + local_tile.x;
	if (index < 0 || index > cache->tile_count) {
		return 0;
	} else {
		return &cache->tiles[index];
	}
}

Entity* entity_at_tile(TileEntityCache* cache, Tile tile) {
	Vector2i local_tile = world_tile_to_local_map(tile);
	int index = local_tile.y * map.width + local_tile.x;
	if (index < 0 || index > cache->tile_count) {
		return 0;
	} else {
		return cache->tiles[index].entity;
	}
}

// :map init
void world_setup()
{
	Entity* player_en = entity_create();
	setup_player(player_en);
	player_en->pos.x = 20.f;

	Entity* en = entity_create();
	setup_oxygenerator(en);
	world->oxygenerator = en;
	en->pos = snap_position_to_nearest_tile_based_on_arch(v2(0, 0), en->arch);

	en = entity_create();
	setup_workbench(en);
	en->pos = snap_position_to_nearest_tile_based_on_arch(v2(-tile_width, tile_width), en->arch);

	world->item_unlocks[ITEM_research_station].research_progress = 100;
	world->item_unlocks[ITEM_workbench].research_progress = 100;

	// :test stuff
	#if defined(DEV_TESTING)
	{
		player_en->exp_amount = 1000;
		
		world->item_unlocks[ITEM_tether].research_progress = 100;

		world->inventory_items[ITEM_iron_ingot].amount = 100;
		world->inventory_items[ITEM_raw_copper].amount = 50;
		world->inventory_items[ITEM_pine_wood].amount = 50;
		world->inventory_items[ITEM_coal].amount = 50;
		world->inventory_items[ITEM_o2_shard].amount = 50;
		world->inventory_items[ITEM_rock].amount = 1000;
		world->inventory_items[ITEM_exp].amount = 100;
		world->inventory_items[ITEM_flint_axe].amount = 1;
		world->inventory_items[ITEM_flint].amount = 100;
		world->inventory_items[ITEM_fiber].amount = 100;
		world->inventory_items[ITEM_copper_ingot].amount = 100;

		// en = entity_create();
		// setup_furnace(en);
		// en->pos.y = 20.0;

		// en = entity_create();
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
	// PARTICLE_FLAGS_gravity = (1<<3),
	// PARTICLE_FLAGS_bounce = (1<<4),
} ParticleFlags;
typedef struct Particle {
	ParticleFlags flags;
	Vector4 col;
	Vector2 pos;
	Vector2 velocity;
	Vector2 acceleration;
	float friction;
	float64 end_time;
	float fade_out_vel_range;
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

		if (p->end_time && has_reached_end_time(p->end_time)) {
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

		draw_rect(p->pos, v2(1, 1), col);
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
	return true;
}

// :ui
void do_ui_stuff() {
	set_screen_space();
	push_z_layer(layer_ui);

	// "screen space"
	Vector2 text_scale = v2(0.1, 0.1);
	Vector4 bg_col = v4(0, 0, 0, 0.90);
	Vector4 fill_col = v4(0.5, 0.5, 0.5, 1.0);
	Vector4 accent_col = hex_to_rgba(0x44c3daff);

	// :exp amount
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

	// oxygenerator ui
	// first attempt
	/*
	if (world->ux_state == UX_oxygenerator && world->interacting_with_entity)
	{
		world_frame.hover_consumed = true;
		Vector2 bg_size = {60, 60};

		float x_left = screen_width * 0.5 - bg_size.x * 0.5;
		float y_top = screen_height * 0.5 + bg_size.y * 0.5;
		float x_middle = x_left + bg_size.x * 0.5;
		float y_bottom = y_top - bg_size.y;

		float x0 = x_left;
		float y0 = y_bottom;

		draw_rect(v2(x0, y0), bg_size, bg_col);

		// slot in center, can input shards into it
		{
			x0 = x_middle;
			y0 = y_bottom + bg_size.y * 0.5;

			Vector2 slot_size = { 10, 10 };
			x0 -= slot_size.x * 0.5;
			y0 -= slot_size.y * 0.5;

			Range2f slot = range2f_make_bottom_left(v2(x0, y0), slot_size);

			draw_rect(slot.min, slot_size, COLOR_GRAY);
		}

		// progress meter of how much fuel is left
		{
			x0 = x_middle;
			y0 -= 4.f;

			Vector2 size = { bg_size.x * 0.8, 4.f };
			x0 -= size.x * 0.5;
			y0 -= size.y;

			draw_rect(v2(x0, y0), size, COLOR_WHITE);
		}
	}
	*/

	// :respawn ui
	if (world->ux_state == UX_respawn) {
		u32 title_font_height = 128;
		u32 subtitle_font_height = 64;

		float x0 = screen_width * 0.5;
		float y0 = screen_height * 0.6666;
		draw_text_with_pivot(font, STR("DED"), title_font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_bottom_center);

		y0 -= 5.0;
		draw_text_with_pivot(font, STR("Half the EXP was lost"), subtitle_font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);

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
			float y_pos = screen_height - 14.f;

			int item_count = 0;
			for (int i = 0; i < ARCH_MAX; i++) {
				InventoryItemData* item = &world->inventory_items[i];
				if (item->amount > 0) {
					item_count += 1;
				}
			}

			const float icon_thing = 8.0;
			float icon_width = icon_thing;

			const int icon_row_count = 8;

			float entire_thing_width_idk = icon_row_count * icon_width;
			float x_start_pos = (screen_width/2.0)-(entire_thing_width_idk/2.0);

			// bg box rendering thing
			{
				Vector2 size = v2(entire_thing_width_idk, icon_width);
				Range2f box = range2f_make_bottom_left(v2(x_start_pos, y_pos), size);
				draw_rect(box.min, size, bg_col);

				// put :cursor item back
				if (world->mouse_cursor_item.id) {
					is_inventory_enabled = false;
					if (range2f_contains(box, get_mouse_pos_in_world_space())) {
						world_frame.hover_consumed = true;
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							world->inventory_items[world->mouse_cursor_item.id].amount += world->mouse_cursor_item.amount;
							world->mouse_cursor_item = (ItemAmount){0};
						}
					}
				}
			}


			int slot_index = 0;
			ItemID hovered_item = 0;
			for (int i = 1; i < ITEM_MAX; i++) {
				ItemData item_data = get_item_data(i);
				InventoryItemData* item = &world->inventory_items[i];
				if (item->amount > 0) {

					float slot_index_offset = slot_index * icon_width;

					Matrix4 xform = m4_scalar(1.0);
					xform = m4_translate(xform, v3(x_start_pos + slot_index_offset, y_pos, 0.0));

					Sprite* sprite = get_sprite(get_sprite_id_from_item(i));

					float is_selected_alpha = 0.0;

					Range2f box = range2f_make_bottom_left(v2(x_start_pos + slot_index_offset, y_pos), v2(8, 8));

					Draw_Quad* quad = draw_rect_xform(xform, v2(8, 8), v4(1, 1, 1, 0.2));
					Range2f icon_box_ndc = quad_to_range(*quad);
					if (is_inventory_enabled && range2f_contains(icon_box_ndc, get_mouse_pos_in_ndc())) {
						is_selected_alpha = 1.0;
						hovered_item = i;
					}

					Matrix4 box_bottom_right_xform = xform;

					xform = m4_translate(xform, v3(icon_width * 0.5, icon_width * 0.5, 0.0));

					if (is_selected_alpha == 1.0)
					{
						float scale_adjust = 0.3; //0.1 * sin_breathe(os_get_elapsed_seconds(), 20.0);
						xform = m4_scale(xform, v3(1 + scale_adjust, 1 + scale_adjust, 1));
					}
					{
						// could also rotate ...
						// float adjust = PI32 * 2.0 * sin_breathe(os_get_elapsed_seconds(), 1.0);
						// xform = m4_rotate_z(xform, adjust);
					}
					xform = m4_translate(xform, v3(get_sprite_size(sprite).x * -0.5, get_sprite_size(sprite).y * -0.5, 0));

					ItemAmount item_amount = (ItemAmount){i, item->amount};
					draw_item_amount_in_rect(item_amount, box);

					// draw_text_xform(font, STR("5"), font_height, box_bottom_right_xform, v2(0.1, 0.1), COLOR_WHITE);

					// tooltip
					if (is_selected_alpha == 1.0)
					{
						Draw_Quad screen_quad = ndc_quad_to_screen_quad(*quad);
						Range2f screen_range = quad_to_range(screen_quad);
						Vector2 icon_center = range2f_get_center(screen_range);

						// icon_center
						Matrix4 xform = m4_scalar(1.0);

						// TODO - we're guessing at the Y box size here.
						// in order to automate this, we would need to move it down below after we do all the element advance things for the text.
						// ... but then the box would be drawing in front of everyone. So we'd have to do Z sorting.
						// Solution for now is to just guess at the size and OOGA BOOGA.
						Vector2 box_size = v2(40, 14);

						// xform = m4_pivot_box(xform, box_size, PIVOT_top_center);
						xform = m4_translate(xform, v3(box_size.x * -0.5, -box_size.y - icon_width * 0.5, 0));
						xform = m4_translate(xform, v3(icon_center.x, icon_center.y, 0));
						draw_rect_xform(xform, box_size, bg_col);

						float current_y_pos = icon_center.y;
						{
							string title = item_data.pretty_name;
							Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));
							Vector2 draw_pos = icon_center;
							draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
							draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center

							draw_pos = v2_add(draw_pos, v2(0, icon_width * -0.5));
							draw_pos = v2_add(draw_pos, v2(0, -2.0)); // padding

							draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

							current_y_pos = draw_pos.y;
						}

						{
							string text = STR("x%i");
							text = tprint(text, item->amount);

							Gfx_Text_Metrics metrics = measure_text(font, text, font_height, v2(0.1, 0.1));
							Vector2 draw_pos = v2(icon_center.x, current_y_pos);
							draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
							draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center

							draw_pos = v2_add(draw_pos, v2(0, -2.0)); // padding

							draw_text(font, text, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
						}
					}

					slot_index += 1;
				}
			}

			// :cursor item
			if (!world->mouse_cursor_item.id && hovered_item) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					InventoryItemData* item = &world->inventory_items[hovered_item];
					world->mouse_cursor_item = (ItemAmount){.id=hovered_item, .amount=item->amount};
					item->amount = 0;
				}
			}
		}
	}

	// :workbench
	if (world->ux_state == UX_entity_interaction && world->interacting_with_entity->arch == ARCH_workbench) {
		Entity* entity = world->interacting_with_entity;

		Vector2 pane_size = v2(60.0, 70.0);
		float text_height_pad = 4.0;

		float x0 = screen_width * 0.5 - pane_size.x * 0.5;
		float y0 = screen_height * 0.5 - pane_size.y * 0.5;
		float x_left = x0;
		float x_middle = x0 + pane_size.x * 0.5;

		Range2f rect = range2f_make_bottom_left(v2(x0, y0), pane_size);
		if (range2f_contains(rect, get_mouse_pos_in_world_space())) {
			world_frame.hover_consumed = true;
		}

		draw_rect(v2(x0, y0), pane_size, bg_col);

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
		ItemID selected = 0;
		int count = 0;
		for (ItemID i = 1; i < ITEM_MAX; i++) {
			ItemData item_data  = get_item_data(i);
			bool unlocked = is_fully_unlocked(world->item_unlocks[i]);
			if (item_data.ingredients_count == 0 || item_data.disabled) {
				continue;
			}

			count += 1;

			{
				Range2f box = range2f_make_bottom_left(v2(x0, y0), v2(icon_length, icon_length));

				Matrix4 xform = m4_identity;

				float scale = 1.f;
				if (unlocked && range2f_contains(box, get_mouse_pos_in_world_space())) {
					scale = 1.2f;
					selected = i;
				}

				xform = m4_translate(xform, v3(x0, y0, 1));
				xform = m4_translate(xform, v3(icon_length * 0.5, icon_length * 0.5, 1));
				xform = m4_scale(xform, v3(scale, scale, 1));
				xform = m4_translate(xform, v3(icon_length * -0.5, icon_length * -0.5, 1));

				Range2f render_box = range2f_make_bottom_left(v2(0, 0), v2(icon_length, icon_length));

				Draw_Quad* quad = draw_sprite_in_rect_with_xform(item_data.icon, render_box, COLOR_WHITE, 0.2, xform);
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
		defer_scope(push_z_layer(layer_tooltip), pop_z_layer()) {
			UnlockState unlock_state = world->item_unlocks[selected];
			ItemData item_data = get_item_data(selected);

			if (is_fully_unlocked(unlock_state)) {
				bool has_enough_for_recipe = true;
				for (int i = 0; i < item_data.ingredients_count; i++) {
					ItemAmount ing = item_data.ingredients[i];
					ItemData ing_data = get_item_data(ing.id);
					if (world->inventory_items[ing.id].amount < ing.amount) {
						has_enough_for_recipe = false;
					}
				}

				// craft
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);

					if (has_enough_for_recipe) {
						// insta craft straight into cursor
						// #future, we'll wanna make this craft queue so we can lean into automated crafting
						if (!world->mouse_cursor_item.id || world->mouse_cursor_item.id == selected) {
							world->mouse_cursor_item.id = selected;
							world->mouse_cursor_item.amount += 1;

							for (int i = 0; i < item_data.ingredients_count; i++) {
								ItemAmount ing = item_data.ingredients[i];
								ItemData ing_data = get_item_data(ing.id);
								world->inventory_items[ing.id].amount -= ing.amount;
							}
						}

						play_sound("event:/craft");
					} else {
						play_sound("event:/error");
					}

				}

				// tooltip
				{
					float width = 40.f;

					float x_left = get_mouse_pos_in_world_space().x;
					float y_top = get_mouse_pos_in_world_space().y;

					float x_middle = x_left + width * 0.5;

					float x0 = x_left;
					float y0 = y_top;

					float padding = 2.f;

					x0 = x_left + width * 0.5;
					y0 = y_top;
					y0 -= padding;

					// title
					{
						string txt = get_archetype_data(item_data.to_build).pretty_name;
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
					for (int i = 0; i < item_data.ingredients_count; i++) {
						ItemAmount ing = item_data.ingredients[i];
						ItemData ing_data = get_item_data(ing.id);

						float height = font_height_body * text_scale.x;

						x0 = x_left_ingredient_list;

						Vector4 col = COLOR_WHITE;
						if (world->inventory_items[ing.id].amount < ing.amount) {
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

					Draw_Quad* quad = draw_rect(v2(x_left, y_top), v2(width, height), v4(0.2, 0.2, 0.2, 1.));
					quad->z = layer_tooltip-1;
				}
			}
		}
	}

	// :research ui
	if (world->ux_state == UX_research) {
		Entity* entity = world->interacting_with_entity;

		Vector2 pane_size = v2(60.0, 70.0);
		float text_height_pad = 4.0;

		float x0 = screen_width * 0.5 - pane_size.x * 0.5;
		float y0 = screen_height * 0.5 - pane_size.y * 0.5;
		float x_left = x0;
		float x_middle = x0 + pane_size.x * 0.5;

		draw_rect(v2(x0, y0), pane_size, bg_col);

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
		ItemID selected = 0;
		int count = 0;
		for (int i = 1; i < ITEM_MAX; i++) {
			UnlockState unlock_state = world->item_unlocks[i];
			ItemData item_data = get_item_data(i);
			if (item_data.ingredients_count == 0 || item_data.disabled || is_fully_unlocked(unlock_state)) {
				continue;
			}

			count += 1;

			{
				Range2f box = range2f_make_bottom_left(v2(x0, y0), v2(icon_length, icon_length));

				Matrix4 xform = m4_identity;

				float scale = 1.f;
				if (range2f_contains(box, get_mouse_pos_in_world_space())) {
					scale = 1.2f;
					selected = i;
				}

				Range2f render_box = range2f_make_bottom_left(v2(0, 0), v2(icon_length, icon_length));
				xform = m4_translate(xform, v3(x0, y0, 1));
				xform = m4_translate(xform, v3(icon_length * 0.5, icon_length * 0.5, 1));
				xform = m4_scale(xform, v3(scale, scale, 1));
				xform = m4_translate(xform, v3(icon_length * -0.5, icon_length * -0.5, 1));

				draw_sprite_in_rect_with_xform(item_data.icon, render_box, COLOR_WHITE, 0.2, xform);
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
			ItemData item_data = get_item_data(selected);

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if (get_player()->exp_amount >= item_data.exp_cost) {
					get_player()->exp_amount -= item_data.exp_cost;
					unlock_state->research_progress = 100;
					play_sound("event:/research");
				} else {
					play_sound("event:/error");
					exp_error_flash_alpha_target = 1.0f;
				}
			}

			Vector2 size = v2(40, 30);

			float x_left = get_mouse_pos_in_world_space().x;
			float y_top = get_mouse_pos_in_world_space().y;
			float x_middle = x_left + size.x * 0.5;
			float y_bottom = y_top - size.y;

			float x0 = x_left;
			float y0 = y_top;

			y0 = y_bottom;
			draw_rect(v2(x0, y0), size, v4(0.2, 0.2, 0.2, 1.));

			x0 = x_left + size.x * 0.5;
			y0 = y_top;
			y0 -= 2.f;

			// title
			{
				string txt = get_archetype_data(item_data.to_build).pretty_name;
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

			// exp cost
			{
				Vector4 col = COLOR_WHITE;
				if (exp_error_flash_alpha) {
					col.xyz = v3_lerp(col.xyz, COLOR_RED.xyz, exp_error_flash_alpha);
				}
				string txt = tprint("Costs: %iml", item_data.exp_cost);
				Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, col, PIVOT_bottom_center);
			}
		}
	}


	// cursor 'Q' quit
	if (world->mouse_cursor_item.id) {
		if (is_key_just_pressed('Q')) {
			consume_key_just_pressed('Q');
			world->inventory_items[world->mouse_cursor_item.id].amount += world->mouse_cursor_item.amount;
			world->mouse_cursor_item = (ItemAmount){0};
		}
	}

	// :cursor item drawing
	if (world->mouse_cursor_item.id)
	defer_scope(push_z_layer(layer_cursor_item), pop_z_layer())
	{
		ItemData item_data = get_item_data(world->mouse_cursor_item.id);
		if (!item_data.to_build || world_frame.hover_consumed) {
			// it's just an item
			Sprite* sprite = get_sprite(get_sprite_id_from_item(world->mouse_cursor_item.id));
			Range2f rect = range2f_make_center_center(get_mouse_pos_in_world_space(), v2(10, 10));
			draw_item_amount_in_rect(world->mouse_cursor_item, rect);
		} else
			defer_scope(set_world_space(), set_screen_space())
			defer_scope(push_z_layer(layer_buildings), pop_z_layer())
		{
			// :place building
			world_frame.hover_consumed = true;

			Sprite* icon = get_sprite(item_data.icon);
			ArchetypeID arch_id = item_data.to_build;
			Entity arch_data = get_archetype_data(arch_id);

			Vector2 pos = get_mouse_pos_in_world_space();
			pos = snap_position_to_nearest_tile_based_on_arch(pos, arch_id);

			// range preview
			if (arch_id == ARCH_burner_drill)
			{
				float radius = get_archetype_data(arch_id).radius;
				// #polish - make this an outline, copy custom shader example
				draw_circle(v2_sub(pos, v2(radius, radius)), v2(radius*2, radius*2), v4(1, 1, 1, 0.1));
			}

			// draw preview
			{
				Matrix4 xform = m4_identity;
				Vector2 sprite_size = get_sprite_size(icon);
				xform = m4_translate(xform, v3(pos.x, pos.y, 0));
				xform = m4_translate(xform, v3(sprite_size.x * -0.5, sprite_size.y * -0.5, 0));

				draw_image_xform(icon->image, xform, get_sprite_size(icon), COLOR_WHITE);
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
				Entity* en = entity_create();
				entity_setup(en, arch_id);
				en->pos = pos;
				en->right_click_remove = true;
				en->item_id = world->mouse_cursor_item.id;

				world->mouse_cursor_item.amount -= 1;
				if (world->mouse_cursor_item.amount <= 0) {
					world->mouse_cursor_item = (ItemAmount){0};
				}
			}
		}
	}

	// esc exit
	if (world->ux_state != UX_nil && is_key_just_pressed(KEY_ESCAPE)) {
		consume_key_just_pressed(KEY_ESCAPE);
		world->ux_state = 0;
	}

	set_world_space();
	pop_z_layer();
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

	// :sound init
	fmod_init();
	#if defined(LOOP_SOUND)
	play_sound("event:/bg_loop");
	#endif

	// :col
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
		sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree_beeg.png"), get_heap_allocator()) };
		sprites[SPRITE_rock0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/rock0.png"), get_heap_allocator()) };
		sprites[SPRITE_item_pine_wood] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_pine_wood.png"), get_heap_allocator()) };
		sprites[SPRITE_item_rock] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_rock.png"), get_heap_allocator()) };
		sprites[SPRITE_furnace] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/furnace.png"), get_heap_allocator()) };
		sprites[SPRITE_workbench] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/workbench.png"), get_heap_allocator()) };
		sprites[SPRITE_research_station] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/research_station.png"), get_heap_allocator()) };
		sprites[SPRITE_exp] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/exp.png"), get_heap_allocator()) };
		sprites[SPRITE_exp_vein] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/exp_vein.png"), get_heap_allocator()) };
		sprites[SPRITE_teleporter1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/teleporter1.png"), get_heap_allocator()) };
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
	
	// item data resource setup
	{
		// do defaults
		for (ItemID i = 1; i < ITEM_MAX; i++) {
			ItemData* data = &item_data[i];
			data->craft_length = 2.0;
		}

		// :item
		item_data[ITEM_iron_ingot] = (ItemData){ .pretty_name=STR("Iron Ingot"), .icon=SPRITE_iron_ingot};
		item_data[ITEM_raw_iron] = (ItemData){ .pretty_name=STR("Raw Iron"), .icon=SPRITE_raw_iron, .furnace_transform_into=ITEM_iron_ingot};
		item_data[ITEM_o2_shard] = (ItemData){ .pretty_name=STR("Oxygen Shard"), .icon=SPRITE_o2_shard};
		item_data[ITEM_exp] = (ItemData){ .pretty_name=STR("Old Rock Thing"), .icon=SPRITE_exp};
		item_data[ITEM_rock] = (ItemData){ .pretty_name=STR("Rock"), .icon=SPRITE_item_rock };
		item_data[ITEM_pine_wood] = (ItemData){ .pretty_name=STR("Pine Wood"), .icon=SPRITE_item_pine_wood, .furnace_transform_into=ITEM_coal };
		item_data[ITEM_raw_copper] = (ItemData){ .pretty_name=STR("Raw Copper"), .icon=SPRITE_raw_copper, .furnace_transform_into=ITEM_copper_ingot };
		item_data[ITEM_fiber] = (ItemData){ .pretty_name=STR("Fiber"), .icon=SPRITE_fiber };
		item_data[ITEM_flint] = (ItemData){ .pretty_name=STR("Flint"), .icon=SPRITE_flint };

		item_data[ITEM_copper_ingot] = (ItemData){
			.pretty_name=STR("Copper Ingot"),
			.description=STR("Shiny"),
			.icon=SPRITE_copper_ingot,
			.craft_length=20,
			.for_structure=ARCH_furnace,
		};

		item_data[ITEM_coal] = (ItemData){
			.pretty_name=STR("Coal"),
			.description=STR("FUEEEEEEL"),
			.icon=SPRITE_coal,
			.craft_length=5,
			.for_structure=ARCH_furnace,
		};

		// NOTE
		// maybe we move some of these guys into the first round of item researching??
		//
		item_data[ITEM_flint_axe] = (ItemData){
			.disabled=true,
			.pretty_name=STR("Flint Axe"),
			.description=STR("+1 damage to trees"),
			.icon=SPRITE_flint_axe,
			.craft_length=10,
			.extra_axe_dmg=1,
			.ingredients_count=3,
			.ingredients={
				{ITEM_pine_wood, 10},
				{ITEM_flint, 10},
				{ITEM_fiber, 10},
			},
		};

		item_data[ITEM_flint_pickaxe] = (ItemData){
			.disabled=true,
			.pretty_name=STR("Flint Pickaxe"),
			.description=STR("+1 damage to rocks"), // todo - make this functional from the extra dmg state
			.icon=SPRITE_flint_pickaxe,
			.craft_length=10,
			.extra_pickaxe_dmg=1,
			.ingredients_count=3,
			.ingredients={
				{ITEM_pine_wood, 10},
				{ITEM_flint, 10},
				{ITEM_fiber, 10},
			},
		};

		item_data[ITEM_flint_scythe] = (ItemData){
			.disabled=true,
			.pretty_name=STR("Flint Scythe"),
			.description=STR("+1 damage to grass"),
			.icon=SPRITE_flint_scythe,
			.craft_length=10,
			.extra_sickle_dmg=1,
			.ingredients_count=3,
			.ingredients={
				{ITEM_pine_wood, 10},
				{ITEM_flint, 10},
				{ITEM_fiber, 10},
			},
		};

		// :item :buildings

		item_data[ITEM_o2_emitter] = (ItemData){
			.to_build=ARCH_o2_emitter,
			.icon=SPRITE_o2_emitter,
			.description=STR("Feed oxygen into surrounding blocks"),
			.exp_cost=200,
			.ingredients_count=2,
			.ingredients={ {ITEM_iron_ingot, 5}, {ITEM_copper_ingot, 2} }
		};

		// note, this should go into a "Shelter #1" grouped research unlock thingo
		item_data[ITEM_wall_gate] = (ItemData){
			.to_build=ARCH_wall_gate,
			.icon=SPRITE_wall_gate,
			.description=STR("Allows travel into a sealed room"),
			.exp_cost=200,
			.ingredients_count=1,
			.ingredients={ {ITEM_iron_ingot, 1}, }
		};
		item_data[ITEM_wall] = (ItemData){
			.to_build=ARCH_wall,
			.icon=SPRITE_wall,
			.description=STR("Can be used to build a sealed Oxygen room"),
			.exp_cost=200,
			.ingredients_count=1,
			.ingredients={ {ITEM_iron_ingot, 1} }
		};

		item_data[ITEM_longboi_test] = (ItemData){
			.disabled=true,
			.to_build=ARCH_longboi_test,
			.icon=SPRITE_longboi_test,
			.description=STR("Place on top of resources to mine."),
			.exp_cost=10,
			.ingredients_count=2,
			.ingredients={ {ITEM_rock, 30}, {ITEM_copper_ingot, 5} }
		};

		item_data[ITEM_burner_drill] = (ItemData){
			.to_build=ARCH_burner_drill,
			.icon=SPRITE_burner_drill,
			.description=STR("Burns coal to hit things"),
			.exp_cost=400,
			.ingredients_count=2,
			.ingredients={ {ITEM_rock, 30}, {ITEM_copper_ingot, 5} }
		};

		item_data[ITEM_tether] = (ItemData){
			.to_build=ARCH_tether,
			.icon=SPRITE_tether,
			.description=STR("Extends oxygen range"),
			.exp_cost=50,
			.ingredients_count=2,
			.ingredients={ {ITEM_copper_ingot, 1}, {ITEM_fiber, 4} }
		};

		item_data[ITEM_furnace] = (ItemData){
			.to_build=ARCH_furnace,
			.icon=SPRITE_furnace,
			.description=STR("Can burn stuff into something more useful."),
			.exp_cost=30,
			.ingredients_count=2,
			.ingredients={ {ITEM_rock, 10}, {ITEM_flint, 2} }
		};

		item_data[ITEM_workbench] = (ItemData){
			.disabled=true,
			.to_build=ARCH_workbench,
			.icon=SPRITE_fabricator,
			.description=STR("Crafts things."),
			.ingredients_count=2,
			.ingredients={ {ITEM_pine_wood, 10}, {ITEM_fiber, 5} }
		};

		item_data[ITEM_research_station] = (ItemData){
			.to_build=ARCH_research_station,
			.icon=SPRITE_research_station,
			.description=STR("Research recipes to unlock more buildings."),
			.ingredients_count=2,
			.ingredients={ {ITEM_pine_wood, 2}, {ITEM_rock, 5} }
		};
		item_data[ITEM_teleporter1] = (ItemData){
			.to_build=ARCH_teleporter1,
			.icon=SPRITE_teleporter1,
			.description=STR("A gateway to the next world."),
			.exp_cost=9999,
			.ingredients_count=2,
			.ingredients={ {ITEM_pine_wood, 1000}, {ITEM_rock, 1000} }
		};
	}

	init_biome_maps();

	// the :init zone

	// :shader init
	{
		string source;
		bool ok = os_read_entire_file("res/shader.hlsl", &source, get_heap_allocator());
		assert(ok);
		gfx_shader_recompile_with_extension(source, sizeof(ShaderConstBuffer));
		dealloc_string(get_heap_allocator(), source);
	}

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

	float64 seconds_counter = 0.0;
	s32 frame_count = 0;

	float64 last_time = os_get_elapsed_seconds();
	while (!window.should_close) {
		reset_temporary_storage();
		world_frame = (WorldFrame){0};
		float64 current_time = os_get_elapsed_seconds();
		delta_t = current_time - last_time;
		last_time = current_time;
		os_update();

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

		// find player lol
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid && en->arch == ARCH_player) {
				world_frame.player = en;
			}
		}

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
		draw_frame.enable_z_sorting = true;
		cbuffer = (ShaderConstBuffer){0};
		draw_frame.cbuffer = &cbuffer;

		world_frame.world_proj = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		// :camera
		{
			// camera shake - https://www.youtube.com/watch?v=tu-Qe66AvtY
			camera_trauma -= delta_t;
			camera_trauma = clamp_bottom(camera_trauma, 0);
			float cam_shake = clamp_top(pow(camera_trauma, 3), 1);

			Vector2 target_pos = get_player()->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f);

			world_frame.world_view = m4_identity;

			// randy: these might be ordered incorrectly for the camera shake. Not sure.

			// translate into position
			world_frame.world_view = m4_translate(world_frame.world_view, v3(camera_pos.x, camera_pos.y, 0));

			// translational shake
			float shake_x = max_cam_shake_translate * cam_shake * get_random_float32_in_range(-1, 1);
			float shake_y = max_cam_shake_translate * cam_shake * get_random_float32_in_range(-1, 1);
			world_frame.world_view = m4_translate(world_frame.world_view, v3(shake_x, shake_y, 0));

			// rotational shake
			// float shake_rotate = max_cam_shake_rotate * cam_shake * get_random_float32_in_range(-1, 1);
			// world_frame.world_view = m4_rotate_z(world_frame.world_view, shake_rotate);

			// scale the zoom
			world_frame.world_view = m4_scale(world_frame.world_view, v3(1.0/zoom, 1.0/zoom, 1.0));
		}
		set_world_space();
		push_z_layer(layer_world);

		Vector2 mouse_pos_world = get_mouse_pos_in_world_space();
		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y);

		if (world->ux_state == UX_entity_interaction) {
			world_frame.show_inventory = true;
		}

		do_ui_stuff();

		// world update
		{
			world->time_elapsed += delta_t;
		}

		TileEntityCache* tile_entity_cache = create_tile_entity_pair_cache();

		// create an array of tiles for each biome
		TileData* tiles_for_biome[BIOME_MAX] = {0};
		for (BiomeID biome = 1; biome < BIOME_MAX; biome++)
		{
			TileData* tiles;
			growing_array_init((void**)&tiles, sizeof(TileData), get_temporary_allocator());

			for (int y = 0; y < map.height; y++)
			for (int x = 0; x < map.width; x++)
			{
				BiomeID biome_at_tile = map.tiles[y * map.width + x];
				if (biome_at_tile == biome) {
					TileData tdata = {0};
					tdata.tile = local_map_to_world_tile(v2i(x, y));
					tdata.entity_at_tile = entity_at_tile(tile_entity_cache, tdata.tile);
					growing_array_add((void**)&tiles, &tdata);
				}
			}

			tiles_for_biome[biome] = tiles;
		}

		// spawn_res in world
		{
			for (int i = 0; i < ARRAY_COUNT(world_resources); i++) {
				WorldResourceData data = world_resources[i];

				Tile* potential_spawn_tiles;
				growing_array_init((void**)&potential_spawn_tiles, sizeof(Tile), get_temporary_allocator());
				for (int j = 0; j < growing_array_get_valid_count(tiles_for_biome[data.biome_id]); j++) {
					TileData tile_data = tiles_for_biome[data.biome_id][j];
					if (!tile_data.entity_at_tile) {
						growing_array_add((void**)&potential_spawn_tiles, &tile_data.tile);
					}
				}

				bool should_respawn = biome_at_tile(v2_world_pos_to_tile_pos(get_player()->pos)) == BIOME_core;

				if (should_respawn && has_reached_end_time(world->resource_next_spawn_end_time[i])) {
					// pick from a random spot
					int count = growing_array_get_valid_count(potential_spawn_tiles);
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
						Entity* en = entity_create();
						entity_setup(en, data.arch_id);
						en->pos = spawn_pos;

						float respawn_length = 10.0;
						if (world->time_elapsed < 1.0) {
							respawn_length = 0.0;
						}
						world->resource_next_spawn_end_time[i] = now() + respawn_length;
					}
				}

				/*
				if (data.biome_id == BIOME_forest)
				for (int j = 0; j < growing_array_get_valid_count(potential_spawn_tiles); j++) {
					Tile tile = potential_spawn_tiles[j];
					Draw_Quad* quad = draw_circle(v2_tile_pos_to_world_pos(tile), v2(1, 1), COLOR_GREEN);
					quad->z = 30;
				}
				*/
			}
		}

		if (world->interacting_with_entity && world->ux_state == 0) {
			world->interacting_with_entity = 0;
		}

		// in-game :ui
		if (world->ux_state == UX_entity_interaction && world->interacting_with_entity) {
			push_z_layer(layer_ui);

			// randy: I'm trying out making this UI be more diagetic and in the world.
			// that way we can just communicate the inputs/outputs and have the bare minimum info.
			// Instead of popping up a big ugly UI box.
			// I think it's roughly the same amount of work to do it this way, if proven effective.
			// :oxygenerator ui

			Entity* en = world->interacting_with_entity;

			if (en->arch == ARCH_oxygenerator) {

				float x0 = en->pos.x;
				float y0 = en->pos.y;
				y0 += 6.f;

				Vector2 size = v2(10, 10);
				x0 -= size.x * 0.5;

				bool do_tooltip = false;
				Range2f rect = range2f_make_bottom_left(v2(x0, y0), size);
				if (range2f_contains(rect, get_mouse_pos_in_world_space())) {
					world_frame.hover_consumed = true;
					
					// interact with the slot
					{
						if (world->mouse_cursor_item.id) {
							if (en->input0.id) {
								if (en->input0.id == world->mouse_cursor_item.id) {
									// attempt stack
									if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
										consume_key_just_pressed(MOUSE_BUTTON_LEFT);
										en->input0.amount += world->mouse_cursor_item.amount;
										world->mouse_cursor_item = (ItemAmount){0};
									}
								} else {
									if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
										consume_key_just_pressed(MOUSE_BUTTON_LEFT);
										if (world->mouse_cursor_item.id == ITEM_o2_shard) {
											// swap
											ItemAmount temp = world->mouse_cursor_item;
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
									if (world->mouse_cursor_item.id == ITEM_o2_shard) {
										// place inside
										en->input0 = world->mouse_cursor_item;
										world->mouse_cursor_item = (ItemAmount){0};
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
									en->input0 = (ItemAmount){0};
								}

								do_tooltip = true;
							}
						}
					}
				}

				draw_rect(rect.min, size, COLOR_GRAY);

				if (en->input0.id) {
					draw_item_amount_in_rect(en->input0, rect);
				} else {
					Draw_Quad* quad = draw_sprite_in_rect(get_sprite_id_from_item(ITEM_o2_shard), rect, COLOR_WHITE, 0.1);

					if (do_oxygenerator_error(en)) {
						set_col_override(quad, v4(sin_breathe(os_get_elapsed_seconds(), 6.f),0,0, 0.8));
					} else {
						set_col_override(quad, v4(0,0,0, 0.8));
					}
				}

				if (do_tooltip) {
					item_tooltip(en->input0);
				}
			}

			// :burner ux
			if (en->arch == ARCH_burner_drill) {

				ItemID desired_item = ITEM_coal;

				Entity* en = world->interacting_with_entity;
				float x0 = en->pos.x;
				float y0 = en->pos.y;
				y0 += 10.f;

				Vector2 size = v2(10, 10);
				x0 -= size.x * 0.5;

				bool do_tooltip = false;
				Range2f rect = range2f_make_bottom_left(v2(x0, y0), size);
				if (range2f_contains(rect, get_mouse_pos_in_world_space())) {
					
					// interact with the slot
					{
						if (world->mouse_cursor_item.id) {
							if (en->input0.id) {
								if (en->input0.id == world->mouse_cursor_item.id) {
									// attempt stack
									if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
										consume_key_just_pressed(MOUSE_BUTTON_LEFT);
										en->input0.amount += world->mouse_cursor_item.amount;
										world->mouse_cursor_item = (ItemAmount){0};
									}
								} else {
									if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
										consume_key_just_pressed(MOUSE_BUTTON_LEFT);
										if (world->mouse_cursor_item.id == desired_item) {
											// swap
											ItemAmount temp = world->mouse_cursor_item;
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
										world->mouse_cursor_item = (ItemAmount){0};
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
									en->input0 = (ItemAmount){0};
								}

								do_tooltip = true;
							}
						}
					}
				}

				draw_rect(rect.min, size, COLOR_GRAY);

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

					draw_rect(v2(x0, y0), bar_size, COLOR_BLACK);

					float alpha = float_alpha(en->current_fuel, 0, en->last_fuel_max);

					draw_rect(v2(x0, y0), v2(bar_size.x, bar_size.y * alpha), col_fire);
				}

				if (do_tooltip) {
					item_tooltip(en->input0);
				}
			}

			// :furance ux
			if (en->arch == ARCH_furnace) {

				Entity* en = world->interacting_with_entity;
				float x0 = en->pos.x;
				float y0 = en->pos.y;

				// fuel input
				Vector2 slot_size = v2(10, 10);
				x0 -= slot_size.x * 0.5;
				y0 += get_offset_for_rendering(en).y;
				y0 -= slot_size.x + 1.f;
				{
					ItemID desired_item = ITEM_coal;

					Range2f rect = range2f_make_bottom_left(v2(x0, y0), slot_size);
					if (range2f_contains(rect, get_mouse_pos_in_world_space())) {
						
						// interact with the slot
						{
							if (world->mouse_cursor_item.id) {
								if (en->input0.id) {
									if (en->input0.id == world->mouse_cursor_item.id) {
										// attempt stack
										if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
											consume_key_just_pressed(MOUSE_BUTTON_LEFT);
											en->input0.amount += world->mouse_cursor_item.amount;
											world->mouse_cursor_item = (ItemAmount){0};
										}
									} else {
										if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
											consume_key_just_pressed(MOUSE_BUTTON_LEFT);
											if (world->mouse_cursor_item.id == desired_item) {
												// swap
												ItemAmount temp = world->mouse_cursor_item;
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
											world->mouse_cursor_item = (ItemAmount){0};
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
										en->input0 = (ItemAmount){0};
									}

									item_tooltip(en->input0);
								}
							}
						}
					}

					draw_rect(rect.min, slot_size, COLOR_GRAY);

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
						x0 += slot_size.x + 1.f;

						Vector2 bar_size = v2(2, slot_size.y);

						draw_rect(v2(x0, y0), bar_size, COLOR_BLACK);

						float alpha = float_alpha(en->current_fuel, 0, en->last_fuel_max);

						draw_rect(v2(x0, y0), v2(bar_size.x, bar_size.y * alpha), col_fire);
					}
				}

				// item input
				{
					x0 = en->pos.x;
					y0 = en->pos.y;

					x0 -= slot_size.x * 0.5;
					y0 = get_entity_range(en).max.y;
					y0 += 1.f;

					ItemAmount* slot = &en->input1;

					ItemID* desired_items;
					growing_array_init_reserve((void**)&desired_items, sizeof(ItemID), 1, get_temporary_allocator());
					for (ItemID i = 0; i < ITEM_MAX; i++) {
						ItemData item_data = get_item_data(i);
						if (item_data.furnace_transform_into) {
							growing_array_add((void**)&desired_items, &i);
						}
					}

					//

					Range2f rect = range2f_make_bottom_left(v2(x0, y0), slot_size);
					if (range2f_contains(rect, get_mouse_pos_in_world_space())) {
						
						// interact with the slot
						{

							bool has_desired_item_in_hand = false;
							for (int i = 0; i < growing_array_get_valid_count(desired_items); i++) {
								ItemID desired_item = desired_items[i];
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
											world->mouse_cursor_item = (ItemAmount){0};
										}
									} else {
										if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
											consume_key_just_pressed(MOUSE_BUTTON_LEFT);
											if (has_desired_item_in_hand) {
												// swap
												ItemAmount temp = world->mouse_cursor_item;
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
											world->mouse_cursor_item = (ItemAmount){0};
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
										*slot = (ItemAmount){0};
									}

									item_tooltip(*slot);
								}
							}
						}
					}

					draw_rect(rect.min, slot_size, COLOR_GRAY);

					if (slot->id) {
						draw_item_amount_in_rect(*slot, rect);
					}

					// craft progress bar
					{
						y0 += slot_size.x + 1.f;

						Vector2 bar_size = v2(slot_size.y, 2);

						draw_rect(v2(x0, y0), bar_size, COLOR_BLACK);

						float alpha = float_alpha(en->progress_on_crafting, 0, 100);

						draw_rect(v2(x0, y0), v2(bar_size.x * alpha, bar_size.y), col_oxygen);
					}
				}

			}

			pop_z_layer();
		}

		// select entity
		if (!world_frame.hover_consumed)
		{
			// log("%f, %f", mouse_pos_world.x, mouse_pos_world.y);
			// draw_text(font, sprint(temp, STR("%f %f"), mouse_pos_world.x, mouse_pos_world.y), font_height, mouse_pos_world, v2(0.1, 0.1), COLOR_RED);

			float smallest_dist = 99999;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				bool has_interaction = en->destroyable_world_item
					|| en->interactable_entity
					|| en->right_click_remove;
				// add extra :interact cases here ^^

				if (en->interactable_entity && world->interacting_with_entity == en) {
					continue;
				}

				float dist_to_player = v2_dist(en->pos, get_player()->pos);
				if (en->is_valid && has_interaction && dist_to_player < selection_reach_radius) {
					Sprite* sprite = get_sprite(en->sprite_id);

					int entity_tile_x = world_pos_to_tile_pos(en->pos.x);
					int entity_tile_y = world_pos_to_tile_pos(en->pos.y);

					float dist = fabsf(v2_dist(en->pos, mouse_pos_world));
					if (dist < entity_selection_radius) {
						if (!world_frame.selected_entity || (dist < smallest_dist)) {
							world_frame.selected_entity = en;
							smallest_dist = dist;
						}
					}
				}
			}
		}
		
		// :update entities
		Entity* player = get_player();
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {

				// :furnace update
				if (en->arch == ARCH_furnace) {

					// fuel consume
					if (en->current_fuel <= 0 && en->input0.id && en->input1.id) {
						en->last_fuel_max = 200;
						en->current_fuel = 200;

						en->input0.amount -= 1;
						if (en->input0.amount <= 0) {
							en->input0 = (ItemAmount){0};
						}
					}

					// consume crafting item to start
					if (!en->current_crafting_item && en->current_fuel && en->input1.id) {
						en->current_crafting_item = en->input1.id;

						en->input1.amount -= 1;
						if (en->input1.amount <= 0) {
							en->input1 = (ItemAmount){0};
						}
					}

					// progress the crafting
					if (has_reached_end_time(en->next_crafting_progress_tick_end_time) && en->current_crafting_item && en->current_fuel > 0) {
						en->progress_on_crafting += 1;
						en->current_fuel -= 1;
						en->next_crafting_progress_tick_end_time = now() + 0.1;
						if (en->progress_on_crafting >= 100) {

							ItemData item_data = get_item_data(en->current_crafting_item);

							// :CRAFT!
							Entity* drop = entity_create();
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
							en->last_fuel_max = 20;
							en->current_fuel = 20;

							en->input0.amount -= 1;
							if (en->input0.amount <= 0) {
								en->input0 = (ItemAmount){0};
							}
						}
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
									en->current_fuel -= 1;

									against->white_flash_current_alpha = 1.0;
									particle_emit(against->pos, PFX_hit);
									against->health -= 1;

									play_sound_at_pos("event:/hit_generic", against->pos);

									if (against->health <= 0) {
										do_entity_drops(against);
										entity_destroy(against);
									}
								}
							}

							if (did_hit_something) {
								camera_shake(0.1);
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
							en->input0 = (ItemAmount){0};
						}
						en->oxygen = en->oxygen_max;
					}

					if (en->oxygen > 0) {
						en->frame.is_powered = true;
					}
				}

				// pickup exp
				if (is_player_alive() && en->arch == ARCH_exp_orb) {

					if (has_reached_end_time(en->pick_up_cooldown_end_time)) {
						en->is_being_picked_up = true;
					}

					if (en->is_being_picked_up) {

						// physically accelerate towards the player
						en->has_physics = true;
						en->disable_friction = true;
						Vector2 pick_up_target_pos = player->pos;
						Vector2 target_normal = v2_normalize(v2_sub(pick_up_target_pos, en->pos));
						en->acceleration = v2_mulf(target_normal, 2000.0f);
						float mag = v2_length(en->velocity);
						en->velocity = v2_mulf(target_normal, mag);

						if (v2_dist(en->pos, player->pos) < 2.0f) {
							play_sound("event:/exp_pickup");
							player->exp_amount += en->exp_amount;
							entity_destroy(en);
						}
					}
				}

				// pickup item
				if (is_player_alive() && en->arch == ARCH_item) {

					if (has_reached_end_time(en->pick_up_cooldown_end_time)
					&& fabsf(v2_dist(en->pos, get_player()->pos)) < player_pickup_radius) {
						en->is_being_picked_up = true;
					}

					if (en->is_being_picked_up) {

						// physically accelerate towards the player
						en->has_physics = true;
						en->disable_friction = true;
						Vector2 pick_up_target_pos = player->pos;
						Vector2 target_normal = v2_normalize(v2_sub(pick_up_target_pos, en->pos));
						en->acceleration = v2_mulf(target_normal, 1000.0f);
						float mag = v2_length(en->velocity);
						en->velocity = v2_mulf(target_normal, mag);

						if (v2_dist(en->pos, player->pos) < 2.0f) {
							play_sound("event:/item_pickup");
							world->inventory_items[en->item_id].amount += en->item_amount;
							entity_destroy(en);
						}
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
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (!en->is_valid || !en->has_physics) {
				continue;
			}

			Vector2 next_pos = {0};
			if (en->arch == ARCH_player) {
				next_pos = v2_add(en->pos, v2_mulf(en->frame.input_axis, 70.0 * delta_t));
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

			en->pos = next_pos;
		}

		// debug draw collision bounds
		#if defined(DRAW_BOUNDS)
		{
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid && en->has_collision || en->arch == ARCH_player) {
					Range2f rect = range2f_shift(get_entity_collision_bounds(en), en->pos);
					Draw_Quad* quad = draw_rect(rect.min, range2f_size(rect), v4(1, 0, 0, 0.3));
					quad->z = 1000;
				}
			}
		}
		#endif

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
			if (world->oxygenerator->oxygen > 0)
			{
				Entity** connection_stack;
				growing_array_init_reserve((void**)&connection_stack, sizeof(Entity*), 1, get_temporary_allocator());
				growing_array_add((void**)&connection_stack, &world->oxygenerator);

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

		#if defined(MOUSE_COORDS_DEBUG)
		{
			Vector2 mp = get_mouse_pos_in_world_space();
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

		// for each o2 emitter, run through neighboring wall seals, making them powered
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

					Entity* left = entity_at_tile(tile_entity_cache, v2i_add(v2i(-1, 0), current_tile));
					if (left && left->wall_seal && !left->frame.is_powered) {
						growing_array_add((void**)&stack, &left);
					}

					Entity* right = entity_at_tile(tile_entity_cache, v2i_add(v2i(1, 0), current_tile));
					if (right && right->wall_seal && !right->frame.is_powered) {
						growing_array_add((void**)&stack, &right);
					}

					Entity* top = entity_at_tile(tile_entity_cache, v2i_add(v2i(0, 1), current_tile));
					if (top && top->wall_seal && !top->frame.is_powered) {
						growing_array_add((void**)&stack, &top);
					}

					Entity* bottom = entity_at_tile(tile_entity_cache, v2i_add(v2i(0, -1), current_tile));
					if (bottom && bottom->wall_seal && !bottom->frame.is_powered) {
						growing_array_add((void**)&stack, &bottom);
					}
				}
			}
		}

		// figure out if the player is inside
		bool is_inside = true;
		{
			tile_entity_cache = create_tile_entity_pair_cache();
		
			Vector2i* stack;
			growing_array_init_reserve((void**)&stack, sizeof(Vector2i), map.width*map.height, get_temporary_allocator());

			Vector2i start_tile = v2_world_pos_to_tile_pos(player->pos);
			growing_array_add((void**)&stack, &start_tile);

			while (growing_array_get_valid_count(stack)) {
				Vector2i self = stack[growing_array_get_valid_count(stack)-1];
				growing_array_pop((void**)&stack);

				TileCache* self_tc = tile_cache_at_tile(tile_entity_cache, self);
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

					TileCache* tile_cache = tile_cache_at_tile(tile_entity_cache, tile);
					bool is_wall_seal = tile_cache->entity && tile_cache->entity->wall_seal && tile_cache->entity->frame.is_powered;
					if (!tile_cache->visited && !is_wall_seal) {
						growing_array_add((void**)&stack, &tile);
					}
				}
			}
		}

		// :player specific caveman update
		{
			Entity* oxygenerator = world->oxygenerator;

			Entity* closest_tether = 0;
			float closest_dist = 99999;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* tether = &world->entities[i];
				if (tether->is_valid && tether->is_oxygen_tether && tether->frame.is_powered && tether->arch != ARCH_o2_emitter) {
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
			if (connected_to_tether && !is_inside) {
				if (!is_inside) {
					draw_line(v2_add(closest_tether->pos, closest_tether->tether_connection_offset), player->pos, 1.0f, col_tether);
				}
			}

			player->oxygen = clamp(player->oxygen, 0, player->oxygen_max);

			{
				local_persist FMOD_STUDIO_EVENTINSTANCE* o2_riser = 0;

				if (!connected_to_tether && last_app_frame.connected_to_tether) {
					// just left tether
					play_sound("event:/o2_disconnect");
					#if !defined(DISABLE_O2)
					o2_riser = play_sound("event:/o2_riser");
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
				for (ItemID item_id = 1; item_id < ARRAY_COUNT(world->inventory_items); item_id++) {
					InventoryItemData* inv_item_data = &world->inventory_items[item_id];
					if (inv_item_data->amount > 0) {
						Entity* drop = entity_create();
						setup_item(drop, item_id);
						drop->pos = player->pos;
						drop->item_amount = inv_item_data->amount;
					}
					inv_item_data->amount = 0;
				}

				// remove half of exp
				player->exp_amount *= 0.5;

				world->ux_state = UX_respawn;
			}
		}

		// :selection :interact stuff
		if (is_player_alive())
		{
			Entity* selected_en = world_frame.selected_entity;

			// :click destroy
			if (selected_en && selected_en->destroyable_world_item) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);

					play_sound_at_pos("event:/hit_generic", selected_en->pos);
					selected_en->white_flash_current_alpha = 1.0;
					camera_shake(0.1);
					particle_emit(selected_en->pos, PFX_hit);

					int damage_amount = 1;
					if (selected_en->dmg_type == DMG_axe) {
						for (int i = 0; i < ITEM_MAX; i++) {
							if (world->inventory_items[i].amount) {
								damage_amount += get_item_data(i).extra_axe_dmg;
							}
						}
					} else if (selected_en->dmg_type == DMG_pickaxe) {
						for (int i = 0; i < ITEM_MAX; i++) {
							if (world->inventory_items[i].amount) {
								damage_amount += get_item_data(i).extra_pickaxe_dmg;
							}
						}
					} else if (selected_en->dmg_type == DMG_sickle) {
						for (int i = 0; i < ITEM_MAX; i++) {
							if (world->inventory_items[i].amount) {
								damage_amount += get_item_data(i).extra_sickle_dmg;
							}
						}
					} // #extend_dmg_type_here

					selected_en->health -= damage_amount;
					if (selected_en->health <= 0) {
						camera_shake(0.1); // shake extra on death

						// :drops
						do_entity_drops(selected_en);

						// exp orb drops
						for (int i = 0; i < get_random_int_in_range(2, 3); i++) {
							Entity* orb = entity_create();
							setup_exp_orb(orb);
							orb->pos = selected_en->pos;
							orb->has_physics = true;
							orb->friction = 20.f;
							orb->velocity = v2_normalize(v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1)));
							orb->velocity = v2_mulf(orb->velocity, get_random_float32_in_range(100, 200));
							orb->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.4, 0.6);
						}

						entity_destroy(selected_en);
					}
				}
			}

			// interact research station
			if (selected_en && selected_en->arch == ARCH_research_station) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					world->ux_state = UX_research;
					world->interacting_with_entity = selected_en;
				}
			}

			// generic interact entity
			if (selected_en && world->interacting_with_entity != selected_en && selected_en->interactable_entity) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					world->ux_state = UX_entity_interaction;
					world->interacting_with_entity = selected_en;
				}
			}

			// right click remove
			if (selected_en && selected_en->right_click_remove) {
				if (is_key_just_pressed(MOUSE_BUTTON_RIGHT)) {
					consume_key_just_pressed(MOUSE_BUTTON_RIGHT);

					Entity* drop = entity_create();
					setup_item(drop, selected_en->item_id);
					drop->pos = selected_en->pos;

					entity_destroy(selected_en);
				}
			}
		}

		// :render entities
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {

				switch (en->arch) {

					case ARCH_player: break;

					case ARCH_exp_orb: {
						draw_rect(en->pos, v2(1, 1), col_exp);
					} break;

					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						Matrix4 xform = m4_scalar(1.0);
						if (en->arch == ARCH_item) {
							xform         = m4_translate(xform, v3(0, 2.0 * sin_breathe(os_get_elapsed_seconds(), 5.0), 0));
						}

						// all entities basically have a center center position
						// that way the ->pos is very easily used in other areas intuitively.
						Vector2 draw_pos = v2_add(en->pos, get_offset_for_rendering(en));
						xform = m4_translate(xform, v3(draw_pos.x, draw_pos.y, 0));

						Vector4 col = COLOR_WHITE;
						if (world_frame.selected_entity == en) {
							col = col_select;
						}

						if (en->wall_seal && en->frame.is_powered) {
							col = v4_mul(col, col_oxygen);
						}

						if (en->white_flash_current_alpha != 0) {
							animate_f32_to_target(&en->white_flash_current_alpha, 0, delta_t, 20.0f);
						}

						Draw_Quad* q = draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);
						set_col_override(q, v4(1, 1, 1, en->white_flash_current_alpha));

						// :error flash
						bool do_error_flash = false;
						// :burner drill
						if (en->arch == ARCH_burner_drill && en->last_fuel_max && en->current_fuel == 0) {
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

						break;
					}
				}

				// :oxygenerator render
				if (en->arch == ARCH_oxygenerator) {
					Vector2 size = {2, 5};
					Vector2 draw_pos = en->pos;
					draw_pos.x -= size.x * 0.5;
					draw_pos.y -= 3;

					size.y = size.y * ((float)en->oxygen / (float)en->oxygen_max);

					draw_rect(draw_pos, v2(size.x, size.y), col_oxygen);
				}

				// :health bar
				if (en->health && en->health < en->max_health) {
					Vector2 size = {6, 1};
					Vector2 draw_pos = en->pos;
					draw_pos.x -= size.x * 0.5;

					draw_pos.y += get_offset_for_rendering(en).y;
					draw_pos.y -= 2;

					draw_rect(draw_pos, size, COLOR_BLACK);

					float target_alpha = (float)en->health / (float)en->max_health;

					if (en->health_bar_current_alpha == 0.0) {
						en->health_bar_current_alpha = 1.0;
					}
					animate_f32_to_target(&en->health_bar_current_alpha, target_alpha, delta_t, 30.0f);

					draw_rect(draw_pos, v2(size.x * en->health_bar_current_alpha, size.y), COLOR_WHITE);
				}

				// :tether draw blue thingy
				if (en->arch != ARCH_oxygenerator && en->is_oxygen_tether && en->frame.is_powered) {
					Vector2 draw_pos = v2_add(en->pos, v2(-1, -1));
					draw_pos = v2_add(draw_pos, en->tether_connection_offset);
					draw_rect(draw_pos, v2(2, 2), col_oxygen);
				}
			}
		}

		// render :player
		if (is_player_alive()) {
			Entity* en = get_player();

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
			Draw_Quad* quad = draw_image_xform(sprite->image, xform, v2(size.x, size.y), col);

			u32 anim_sheet_pos_x = en->anim_index * size.x;
			u32 anim_sheet_pos_y = 0;
			quad->uv.x1 = (float32)(anim_sheet_pos_x)/(float32)sprite->image->width;
			quad->uv.y1 = (float32)(anim_sheet_pos_y)/(float32)sprite->image->height;
			quad->uv.x2 = (float32)(anim_sheet_pos_x+size.x) /(float32)sprite->image->width;
			quad->uv.y2 = (float32)(anim_sheet_pos_y+size.y)/(float32)sprite->image->height;
		}

		// :tile :rendering
		scope_z_layer(layer_background)
		{
			int player_tile_x = world_pos_to_tile_pos(get_player()->pos.x);
			int player_tile_y = world_pos_to_tile_pos(get_player()->pos.y);
			int tile_radius_x = 40;
			int tile_radius_y = 30;
			for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; x++) {
				for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; y++) {

					BiomeID biome = biome_at_tile(v2i(x, y));
					if (biome == 0) {
						continue;
					}

					// checkerboard pattern
					Vector4 col = color_0;
					if ((x + (y % 2 == 0) ) % 2 == 0) {
						col.a = 0.9;
					}
					col = v4_lerp(col, biome_col_hex_to_rgba(biome_colors[biome]), 0.1f);
					float x_pos = x * tile_width;
					float y_pos = y * tile_width;
					Draw_Quad* quad = draw_rect(v2(x_pos, y_pos), v2(tile_width, tile_width), col);

					// TileCache* tc = tile_cache_at_tile(tile_entity_cache, v2i(x, y));
					// if (tc->visited) {
					// 	set_col_override(quad, v4(1, 0, 0, 0.2));
					// }
				}
			}

			// draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
		}

		particle_update();
		particle_render();

		// player :hud
		if (is_player_alive())
		{
			Entity* player = get_player();

			// o2 meter
			{
				Vector2 size = {6, 1};
				Vector2 draw_pos = player->pos;
				draw_pos.x -= size.x * 0.5;
				draw_pos.y -= 6.0;
				draw_rect(draw_pos, size, COLOR_BLACK);
				float alpha = (float)player->oxygen / (float)player->oxygen_max;
				draw_rect(draw_pos, v2(size.x * alpha, size.y), col_oxygen);
			}
		}

		gfx_update();
		fmod_update();
		seconds_counter += delta_t;
		frame_count += 1;
		if (seconds_counter > 1.0) {
			// log("fps: %i", frame_count);
			seconds_counter = 0.0;
			frame_count = 0;
		}

		// load/save commands
		// these are at the bottom, because we'll want to have a clean spot to do this to avoid any mid-way operation bugs.
		#if CONFIGURATION == DEBUG
		if (is_key_down(KEY_ALT))
		{
			if (is_key_just_pressed('F')) {
				world_save_to_disk();
				log("saved");
			}
			if (is_key_just_pressed('R')) {
				world_attempt_load_from_disk();
				log("loaded ");
			}
			if (is_key_just_pressed('K') && is_key_down(KEY_SHIFT)) {
				memset(world, 0, sizeof(World));
				world_setup();
				log("reset");
			}
		}
		#endif
	}

	world_save_to_disk();
	fmod_shutdown();

	return 0;
}