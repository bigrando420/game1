// The entry function is at the bottom of this file.
// Go there if you want to read this codebase.
// C needs declarations to be ordered, and header files are a waste of time. So reading this top to bottom isn't recommended.

#include "range.c"
#include "config.c"
#include "easings.c"

#include "fmod_sound.c"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

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
Vector4 color_0;
Vector4 col_oxygen;
Vector4 col_tether;
Vector4 col_exp;
#define COLOR_GRAY v4(0.5, 0.5, 0.5, 1.0)

// :tweaks
float o2_fuel_length = 30.f;
Vector2 tether_connection_offset = {0, 4};
float max_cam_shake_translate = 200.0f;
float max_cam_shake_rotate = 4.0f;
float selection_reach_radius = 20.0f;
float tether_connection_radius = 50.0;
float o2_full_tank_deplete_length = 16.0f; // #volatile length of oxygen riser sfx
float oxygen_regen_tick_length = 0.01;
float oxygen_deplete_tick_length = 0.5f;
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
const s32 layer_ui = 20;
const s32 layer_world = 10;

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
	bool losing_o2;
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
	return roundf(world_pos / (float)tile_width);
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
} Sprite;
typedef enum SpriteID {
	SPRITE_nil,
	SPRITE_player,
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
	// :arch
	ARCH_MAX,
} ArchetypeID;
typedef struct ArchetypeData {
	string pretty_name;
} ArchetypeData;
ArchetypeData archetype_data[ARCH_MAX] = {0};
ArchetypeData get_archetype_data(ArchetypeID id) {
	return archetype_data[id];
}
string get_archetype_pretty_name(ArchetypeID id) {
	return get_archetype_data(id).pretty_name;
}

// :item
typedef struct ItemData {
	string pretty_name;
	string description;
	SpriteID icon;
	int extra_axe_dmg; // #extend_dmg_type_here
	int extra_pickaxe_dmg;
	int extra_sickle_dmg;
	// :recipe crafting
	ArchetypeID for_structure;
	ItemAmount crafting_recipe[8];
	int crafting_recipe_count;
	float craft_length;
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
	bool workbench_thing;
	ItemID current_crafting_item;
	int current_crafting_amount;
	float64 crafting_end_time;
	ItemAmount drops[4];
	int drops_count;
	DamageType dmg_type;
	ItemID selected_crafting_item;
	int oxygen;
	float64 oxygen_deplete_end_time;
	float64 oxygen_regen_end_time;
	bool is_oxygen_tether;
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
	float64 fuel_expire_end_time;

	EntityFrame frame;
	EntityFrame last_frame;
	// :entity
} Entity;
#define MAX_ENTITY_COUNT 1024

// building resource
// NOTE, a "resource" is a thing that we set up during startup, and is constant.
//
// randy: I tried removing this enum ID and just have the system be nameless like the world resource spawning system. But it's too much of a mental backflip when thinking about things. Much easier to just type out the enum name again so we can sitll use it as a handle for everything.
typedef enum BuildingID {
	BUILDING_nil,
	BUILDING_furnace,
	BUILDING_workbench,
	BUILDING_research_station,
	BUILDING_teleporter1,
	BUILDING_tether,
	// :building resource
	BUILDING_MAX,
} BuildingID;
typedef struct BuildingData {
	ArchetypeID to_build;
	SpriteID icon;
	int exp_cost;
	string description;
	ItemAmount ingredients[8];
	int ingredients_count;
} BuildingData;
BuildingData buildings[BUILDING_MAX];
BuildingData get_building_data(BuildingID id) {
	// note, this isn't a pointer, because this is constant resource data, we don't want to modify
	return buildings[id];
}

typedef enum UXState {
	UX_nil,
	UX_inventory,
	UX_building,
	UX_place_mode,
	UX_workbench,
	UX_research,
	UX_respawn,
	UX_oxygenerator,
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
	// :biome
	BIOME_MAX,
} BiomeID;

typedef struct WorldResourceData {
	BiomeID biome_id;
	ArchetypeID arch_id;
	int dist_from_self;
} WorldResourceData;
// NOTE - trying out a new pattern here. That way we don't have to keep writing up enums to index into these guys. If we need dynamic runtime data, just make an array with the count of this array and have it essentially share the index. Like what I've done below in the world state.
WorldResourceData world_resources[] = {
	{ BIOME_forest, ARCH_tree, 10 },
	{ BIOME_forest, ARCH_flint_depo, 15 },
	{ BIOME_forest, ARCH_grass, 10 },
	{ BIOME_forest, ARCH_ice_vein, 20 },

	{ BIOME_barren, ARCH_rock, 10 },
	{ BIOME_barren, ARCH_copper_depo, 10 },
	// :spawn_res system
};

typedef struct Map {
	int width;
	int height;
	BiomeID* tiles;
} Map;
Map map = {0};

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

	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		int index = y * width + x;
		u8* pixel = stb_data + index * channels;

		u8 r = pixel[0];
		u8 g = pixel[1];
		u8 b = pixel[2];
		Vector4 col = {(float)r/255.0f, (float)g/255.0f, (float)b/255.0f, 1.0};
		if (v4_equals(col, COLOR_WHITE)) {
			map.tiles[index] = BIOME_core;
		} else if (v4_equals(col, hex_to_rgba(0x3553b9ff))) {
			map.tiles[index] = BIOME_forest;
		} else if (!v4_equals(col, COLOR_BLACK)) {
			map.tiles[index] = BIOME_barren;
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
	return (Vector2i){x_index, y_index};
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
	BuildingID placing_building;
	Entity* interacting_with_entity;
	BuildingID selected_research_thing;
	UnlockState building_unlocks[BUILDING_MAX];
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

bool is_player_alive() {
	return get_player()->health > 0;
}

int get_max_oxygen() {
	return o2_full_tank_deplete_length / (float)oxygen_deplete_tick_length;
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

	return entity_found;
}

void entity_destroy(Entity* entity) {
	memset(entity, 0, sizeof(Entity));
}

// :setup things

void setup_ice_vein(Entity* en) {
	en->arch = ARCH_ice_vein;
	en->sprite_id = SPRITE_ice_vein;
	en->health = ice_vein_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_o2_shard, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_pickaxe;
}

void setup_exp_orb(Entity* en) {
	en->arch = ARCH_exp_orb;
	en->exp_amount = 1;
}

void setup_tether(Entity* en) {
	en->arch = ARCH_tether;
	en->sprite_id = SPRITE_tether;
	en->is_oxygen_tether = true;
	en->right_click_remove = true;
}

void setup_oxygenerator(Entity* en) {
	en->arch = ARCH_oxygenerator;
	en->sprite_id = SPRITE_oxygenerator;
	en->is_oxygen_tether = true;
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
	en->sprite_id = SPRITE_flint_depo;
	en->health = flint_depo_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_flint, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_pickaxe;
}

void setup_copper_depo(Entity* en) {
	en->arch = ARCH_copper_depo;
	en->sprite_id = SPRITE_copper_depo;
	en->health = copper_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_raw_copper, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_pickaxe;
}

void setup_teleporter1(Entity* en) {
	en->arch = ARCH_teleporter1;
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
	en->sprite_id = SPRITE_furnace;
	en->workbench_thing = true;
	en->right_click_remove = true;
}

void setup_workbench(Entity* en) {
	en->arch = ARCH_workbench;
	en->sprite_id = SPRITE_workbench;
	en->workbench_thing = true;
	en->right_click_remove = true;
}

void setup_research_station(Entity* en) {
	en->arch = ARCH_research_station;
	en->sprite_id = SPRITE_research_station;
	en->right_click_remove = true;
}

void setup_player(Entity* en) {
	en->arch = ARCH_player;
	en->sprite_id = SPRITE_player;
	en->health = 1;
	en->max_health = en->health;
	en->oxygen = get_max_oxygen();
}

void setup_rock(Entity* en) {
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock0;
	en->health = rock_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_rock, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_pickaxe;
}

void setup_tree(Entity* en) {
	en->arch = ARCH_tree;
	en->sprite_id = SPRITE_tree1;
	// en->sprite_id = SPRITE_tree1;
	en->health = tree_health;
	en->max_health = en->health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_pine_wood, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_axe;
}

void setup_item(Entity* en, ItemID item_id) {
	en->arch = ARCH_item;
	en->sprite_id = get_sprite_id_from_item(item_id);
	en->item_id = item_id;
	en->item_amount = 1;
	en->isnt_a_tile = true;
}

void entity_setup(Entity* en, ArchetypeID id) {
	switch (id) {
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
		// :arch :setup
		default: log_error("missing entity_setup case entry"); break;
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

// pad_pct just shrinks the rect by a % of itself ... 0.2 is a nice default
Draw_Quad* draw_sprite_in_rect(SpriteID sprite_id, Range2f rect, Vector4 col, float pad_pct) {
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

	return draw_image(sprite->image, rect.min, range2f_size(rect), col);
}

// :func dump

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

}

void camera_shake(float amount) {
	camera_trauma += amount;
}

typedef struct TileCache {
	Entity* entity;
	// BiomeID biome;
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
		assert(index >= 0 && index < cache->tile_count);
		cache->tiles[index].entity = en;
	}

	return cache;
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

inline float64 now() {
	return world->time_elapsed;
}

float alpha_from_end_time(float64 end_time, float length) {
	return float_alpha(now(), end_time-length, end_time);
}

bool has_reached_end_time(float64 end_time) {
	return now() > end_time;
}

// :map init
void world_setup()
{
	Entity* player_en = entity_create();
	setup_player(player_en);

	Entity* en = entity_create();
	setup_oxygenerator(en);
	world->oxygenerator = en;
	en->fuel_expire_end_time = now() + o2_fuel_length;

	world->building_unlocks[BUILDING_research_station].research_progress = 100;
	world->building_unlocks[BUILDING_workbench].research_progress = 100;

	// :test stuff
	#if defined(DEV_TESTING)
	{
		world->building_unlocks[BUILDING_tether].research_progress = 100;

		world->inventory_items[ITEM_pine_wood].amount = 50;
		world->inventory_items[ITEM_rock].amount = 1000;
		world->inventory_items[ITEM_exp].amount = 100;
		world->inventory_items[ITEM_flint_axe].amount = 1;
		world->inventory_items[ITEM_flint].amount = 100;
		world->inventory_items[ITEM_fiber].amount = 100;
		world->inventory_items[ITEM_copper_ingot].amount = 100;

		en = entity_create();
		setup_furnace(en);
		en->pos.y = 20.0;

		en = entity_create();
		setup_research_station(en);
		en->pos.x = -20.0;
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

	// :oxygenerator ui
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
			player->oxygen = get_max_oxygen();
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
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT) && range2f_contains(box, get_mouse_pos_in_world_space())) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						world->inventory_items[world->mouse_cursor_item.id].amount += world->mouse_cursor_item.amount;
						world->mouse_cursor_item = (ItemAmount){0};
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

					Draw_Quad* quad = draw_rect_xform(xform, v2(8, 8), v4(1, 1, 1, 0.2));
					Range2f icon_box = quad_to_range(*quad);
					if (is_inventory_enabled && range2f_contains(icon_box, get_mouse_pos_in_ndc())) {
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
					draw_image_xform(sprite->image, xform, get_sprite_size(sprite), COLOR_WHITE);

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

			// :cusor item
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

	// :building ui
	{
		if (is_key_just_pressed('C')) {
			consume_key_just_pressed('C');
			world->ux_state = (world->ux_state == UX_building ? UX_nil : UX_building);
		}

		world->building_alpha_target = (world->ux_state == UX_building ? 1.0 : 0.0);
		animate_f32_to_target(&world->building_alpha, world->building_alpha_target, delta_t, 15.0);
		bool is_building_enabled = world->building_alpha_target == 1.0;

		// draw a row of icons for buildable structures
		if (world->building_alpha_target == 1.0) {
			int building_count = BUILDING_MAX-1;

			float icon_size = 12.0;
			float total_box_width = building_count * icon_size;

			float padding = 2.0;
			total_box_width += padding * (building_count + 1);

			BuildingID tooltip_id = 0;
			for (BuildingID i = 1; i < BUILDING_MAX; i++) {
				BuildingData* building = &buildings[i];
				UnlockState unlock_state = world->building_unlocks[i];
				bool is_unlocked = is_fully_unlocked(unlock_state);

				Matrix4 xform = m4_identity;

				float x0 = (screen_width * 0.5) - (total_box_width * 0.5);
				x0 += (i-1) * icon_size;
				x0 += padding * i;
				xform = m4_translate(xform, v3(x0, 10, 0));

				draw_rect_xform(xform, v2(icon_size, icon_size), v4(0, 0, 0, 0.2));

				Sprite* icon = get_sprite(building->icon);
				Vector4 col = COLOR_WHITE;
				if (!is_unlocked) {
					col = v4(0.0, 0.0, 0.0, 1.0);
				}

				Range2f box = range2f_make_bottom_left(v2(x0, 10), v2(icon_size, icon_size));
				draw_sprite_in_rect(building->icon, box, col, 0.1);

				if (is_unlocked && range2f_contains(box, get_mouse_pos_in_world_space())) {
					world_frame.hover_consumed = true;
					tooltip_id = i;

					bool has_all_ingredients = true;
					for (int j = 0; j < building->ingredients_count; j++) {
						ItemAmount ing_amount = building->ingredients[j];
						InventoryItemData inv_item = world->inventory_items[ing_amount.id];
						if (inv_item.amount < ing_amount.amount) {
							has_all_ingredients = false;
							break;
						}
					}

					// if click, go into place mode
					if (has_all_ingredients && is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						world->placing_building = i;
						world->ux_state = UX_place_mode;

						// consume ingredients
						for (int j = 0; j < building->ingredients_count; j++) {
							ItemAmount ing_amount = building->ingredients[j];
							world->inventory_items[ing_amount.id].amount -= ing_amount.amount;
							assert(world->inventory_items[ing_amount.id].amount >= 0, "removed too many items. the check above must have failed!");
						}
					}
				}
			}

			// building tooltip
			if (tooltip_id)
			{
				BuildingData* building = &buildings[tooltip_id];

				Vector2 size = v2(40, 50);
				Vector2 pos = get_mouse_pos_in_world_space();

				draw_rect(pos, size, COLOR_BLACK);

				float x0, y0;
				x0 = pos.x + size.x * 0.5;
				y0 = pos.y + size.y;
				y0 -= 2.f; // arbitrary padding

				Gfx_Text_Metrics met = draw_text_with_pivot(font, get_archetype_pretty_name(building->to_build), font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
				y0 -= met.visual_size.y;
				y0 -= 2.f;

				// description
				if (building->description.count)
				{
					// todo - text wrapping
					float wrap_width = size.x;
					string text = building->description;
					draw_text_with_pivot(font, text, font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
				}
				// todo - advance by whatever the wrapped text box turns out to be...
				y0 -= 30.f;

				// ingredients list
				for (int j = 0; j < building->ingredients_count; j++) {
					ItemAmount ing_amount = building->ingredients[j];
					// ItemData ing_data = get_item_data(ing_amount.id);

					Vector2 element_size = v2(size.x * 0.8, 6.0);

					x0 = pos.x + (size.x - element_size.x) * 0.5;

					// bg box thing
					draw_rect(v2(x0, y0), element_size, fill_col);

					// icon
					{
						float item_icon_length = element_size.y * 0.8;

						float x1 = pos.x + size.x * 0.5 - element_size.y;
						float y1 = y0 + (item_icon_length - element_size.y) * -0.5;

						draw_image(get_sprite(get_sprite_id_from_item(ing_amount.id))->image, v2(x1, y1), v2(item_icon_length, item_icon_length), COLOR_WHITE);
					}

					InventoryItemData inv_item = world->inventory_items[ing_amount.id];
					Vector4 txt_col = COLOR_WHITE;
					if (inv_item.amount < ing_amount.amount) {
						txt_col = COLOR_RED;
					}

					string txt = tprint("%i/%i", inv_item.amount, ing_amount.amount);
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					float center_pos = pos.x + size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, y0 + element_size.y * 0.5);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0, 0.5)));
					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), txt_col);
					// y0 += metrics.visual_size.y;

					y0 -= element_size.y;
					y0 -= 2.0f; // padding @cleanup
				}
			}
		}
	}

	// :placement mode
	// TODO - alpha animation for place mode
	if (world->ux_state == UX_place_mode) {
		// TODO - put this into macro :)
		set_world_space();
		{
			Vector2 mouse_pos_world = get_mouse_pos_in_world_space();
			BuildingData building = get_building_data(world->placing_building);
			Sprite* icon = get_sprite(building.icon);

			Vector2 pos = mouse_pos_world;
			pos = round_v2_to_tile(pos);

			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(pos.x, pos.y, 0));

			// @volatile with entity rendering
			xform = m4_translate(xform, v3(0, tile_width * -0.5, 0));
			xform = m4_translate(xform, v3(get_sprite_size(icon).x * -0.5, 0.0, 0));

			draw_image_xform(icon->image, xform, get_sprite_size(icon), COLOR_WHITE);

			// :tether connection preview
			if (building.to_build == ARCH_tether)
			{
				for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
					Entity* tether = &world->entities[i];
					if (tether->is_valid && tether->last_frame.is_powered) {
						if (v2_dist(tether->pos, pos) < tether_connection_radius) {
							draw_line(v2_add(tether->pos, tether_connection_offset), v2_add(pos, tether_connection_offset), 1.0f, col_tether);
							break;
						}
					}
				}
			}

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);
				Entity* en = entity_create();
				entity_setup(en, building.to_build);
				en->pos = pos;
				world->ux_state = 0;
			}
		}
		set_screen_space();
	}

	// :workbench ui
	if (world->ux_state == UX_workbench && world->interacting_with_entity) {
		Entity* workbench_en = world->interacting_with_entity;

		Vector2 section_size = v2(50.0, 70.0);
		float gap_between_panels = 10.0;
		float text_height_pad = 4.0;

		float ui_width_thing = section_size.x * 2.0 + gap_between_panels;

		float x0 = screen_width * 0.5 - ui_width_thing * 0.5;
		float y0 = screen_height * 0.5 - section_size.y * 0.5;
		// left pane
		{
			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x0, y0, 0));
			draw_rect_xform(xform, section_size, bg_col);

			// title
			float x1 = x0;
			float y1 = y0 + section_size.y;
			{
				string title = get_archetype_pretty_name(workbench_en->arch);
				Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));

				float center_pos = x0 + section_size.x * 0.5;
				Vector2 draw_pos = v2(center_pos, y1);
				draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
				draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center
				draw_pos.y -= text_height_pad;

				draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

				y1 = draw_pos.y; // TODO - workie?
				y1 -= text_height_pad;
				// y1 -= 20.0;
			}

			Vector2 item_icon_size = v2(8, 8);

			y1 -= item_icon_size.y;

			// draw all item recipes
			for (int i = 1; i < ITEM_MAX; i++) {
				ItemData data = get_item_data(i);
				if (data.for_structure == workbench_en->arch && data.crafting_recipe_count) {
					Matrix4 xform = m4_identity;
					xform = m4_translate(xform, v3(x1, y1, 0));

					Vector4 col = COLOR_WHITE;
					if (workbench_en->selected_crafting_item == i) {
						col = COLOR_RED;
					}

					Range2f rect = range2f_make_bottom_left(v2(x1, y1), item_icon_size);
					draw_sprite_in_rect(get_sprite_id_from_item(i), rect, col, 0.1);
					if (range2f_contains(rect, get_mouse_pos_in_world_space())) {
						// ...
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							workbench_en->selected_crafting_item = i;
						}
					}
					x1 += item_icon_size.x;
				}
			}
		}
		
		x0 += section_size.x;
		x0 += gap_between_panels;
		Vector2 bottom_left_right_pane = v2(x0, y0);
		Vector2 top_left_right_pane = v2(x0, y0 + section_size.y);

		// right pane
		{
			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x0, y0, 0));
			draw_rect_xform(xform, section_size, bg_col);

			if (workbench_en->selected_crafting_item) {
				ItemData selected_item_data = get_item_data(workbench_en->selected_crafting_item);

				// title of item
				y0 += section_size.y;
				{
					string title = selected_item_data.pretty_name;
					Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));

					float center_pos = x0 + section_size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, y0);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center
					draw_pos.y -= text_height_pad;

					draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

					y0 = draw_pos.y; // TODO - workie?
					y0 -= text_height_pad;
					// y1 -= 20.0;
				}

				// description
				{
					x0 += section_size.x * 0.5;

					float wrap_width = section_size.x;
					string text = selected_item_data.description;

					string* lines = split_text_to_lines_with_wrapping(text, wrap_width, font, font_height_body, text_scale, true);
					for (int i = 0; i < growing_array_get_valid_count(lines); i++) {
						string line = lines[i];
						Gfx_Text_Metrics metrics = draw_text_with_pivot(font, line, font_height_body, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
						y0 -= metrics.visual_size.y;
					}
				}

				y0 = bottom_left_right_pane.y + 30.0; // @cleanup

				// list out the ingredients
				for (int i = 0; i < selected_item_data.crafting_recipe_count; i++) {
					ItemAmount ingredient_amount = selected_item_data.crafting_recipe[i];
					ItemData ingredient_item_data = get_item_data(ingredient_amount.id);

					Vector2 element_size = v2(section_size.x * 0.8, 6.0);

					x0 = bottom_left_right_pane.x + (section_size.x - element_size.x) * 0.5;

					// bg box thing
					draw_rect(v2(x0, y0), element_size, fill_col);

					// icon
					{
						float item_icon_length = element_size.y * 0.8;

						float x1 = bottom_left_right_pane.x + section_size.x * 0.5 - element_size.y;
						float y1 = y0 + (item_icon_length - element_size.y) * -0.5;

						Matrix4 xform = m4_identity;
						xform = m4_translate(xform, v3(x1, y1, 0));
						Draw_Quad* quad = draw_image_xform(get_sprite(get_sprite_id_from_item(ingredient_amount.id))->image, xform, v2(item_icon_length, item_icon_length), COLOR_WHITE);
						Range2f icon_box = quad_to_range(*quad);
						if (range2f_contains(icon_box, get_mouse_pos_in_ndc())) {
							// ...
						}
					}

					InventoryItemData inv_item = world->inventory_items[ingredient_amount.id];
					Vector4 txt_col = COLOR_WHITE;
					if (inv_item.amount < ingredient_amount.amount) {
						txt_col = COLOR_RED;
					}

					string txt = tprint("%i/%i", inv_item.amount, ingredient_amount.amount);
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					float center_pos = bottom_left_right_pane.x + section_size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, y0 + element_size.y * 0.5);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0, 0.5)));
					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), txt_col);
					// y0 += metrics.visual_size.y;

					y0 -= element_size.y;
					y0 -= 2.0f; // padding @cleanup
				}

				// craft button
				{
					Vector2 size = v2(section_size.x * 0.8, 6.0);

					x0 = bottom_left_right_pane.x + (section_size.x - size.x) * 0.5;
					y0 = bottom_left_right_pane.y;
					y0 += 5.0f; // padding from bottom @cleanup

					// check for all ingredients met
					bool has_enough_for_crafting = true;
					for (int i = 0; i < selected_item_data.crafting_recipe_count; i++) {
						ItemAmount ingredient_amount = selected_item_data.crafting_recipe[i];
						if (ingredient_amount.amount > world->inventory_items[ingredient_amount.id].amount) {
							has_enough_for_crafting = false;
							break;
						}
					}

					Range2f btn_range = range2f_make_bottom_left(v2(x0, y0), size);
					Vector4 col = fill_col;
					if (has_enough_for_crafting && range2f_contains(btn_range, get_mouse_pos_in_world_space())) {
						col = COLOR_RED;
						world_frame.hover_consumed = true;
						// TODO - where do we put the state for the animation of the button?
						// Either just manually rip it via the App state, or some kind of hash string thing that's probably overcomplicated as shit.
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							// :craft!
							workbench_en->current_crafting_item = workbench_en->selected_crafting_item;
							workbench_en->current_crafting_amount += 1;
							// remove ingredients from inventory
							for (int i = 0; i < selected_item_data.crafting_recipe_count; i++) {
								ItemAmount ingredient_amount = selected_item_data.crafting_recipe[i];
								world->inventory_items[ingredient_amount.id].amount -= ingredient_amount.amount;
								assert(world->inventory_items[ingredient_amount.id].amount >= 0, "removed too many items. the check above must have failed!");
							}
						}
					}
					// todo - disable button with has_enough_for_crafting
					draw_rect(v2(x0, y0), size, col);

					string txt = STR("CRAFT");
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					float center_pos = bottom_left_right_pane.x + section_size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, y0 + size.y * 0.5);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0.5, 0.5)));

					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
				}

			} else {
				// select item first text
				y0 += section_size.y * 0.5;
				{
					string title = STR("Select Item to Craft");
					Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));

					Vector2 draw_pos = v2(x0 + section_size.x * 0.5, y0);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0.5, 0.5)));

					draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
				}
			}
		}

		world_frame.hover_consumed = true;
	}

	// :research ui
	if (world->ux_state == UX_research) {
		Entity* entity = world->interacting_with_entity;

		Vector2 pane_size = v2(60.0, 70.0);
		float text_height_pad = 4.0;

		float x0 = screen_width * 0.5 - pane_size.x * 0.5;
		float y0 = screen_height * 0.5 - pane_size.y * 0.5;

		draw_rect(v2(x0, y0), pane_size, bg_col);

		y0 += pane_size.y;

		float icon_length = 10.f;
		y0 -= icon_length;
		BuildingID selected = 0;
		for (int i = 1; i < BUILDING_MAX; i++) {
			UnlockState unlock_state = world->building_unlocks[i];
			BuildingData building_data = get_building_data(i);
			if (is_fully_unlocked(unlock_state)) {
				continue;
			}

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
				render_box = m4_transform_range2f(xform, render_box);

				draw_sprite_in_rect(building_data.icon, render_box, COLOR_WHITE, 0.2);
			}
			x0 += icon_length;

			// scuffed row advance
			if (i % 5 == 0) {
				y0 -= icon_length;
			}
		}

		if (selected) {
			UnlockState* unlock_state = &world->building_unlocks[selected];
			BuildingData building_data = get_building_data(selected);

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if (get_player()->exp_amount >= building_data.exp_cost) {
					get_player()->exp_amount -= building_data.exp_cost;
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
				string txt = get_archetype_data(building_data.to_build).pretty_name;
				Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_top_center);
				y0 -= met.visual_size.y + 2.f;
			}

			// description
			{
				x0 = x_middle;

				float wrap_width = size.x;
				string text = building_data.description;

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
				string txt = tprint("Costs: %iml", building_data.exp_cost);
				Gfx_Text_Metrics met = draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, col, PIVOT_bottom_center);
			}
		}

		world_frame.hover_consumed = true;
	}

	// :cursor item drawing
	if (world->mouse_cursor_item.id) {
		// Range2f rect = range2f_make_center_center(get_mouse_pos_in_world_space(), v2(10, 10));

		Sprite* sprite = get_sprite(get_sprite_id_from_item(world->mouse_cursor_item.id));
		Vector2 pos = get_mouse_pos_in_world_space();
		Vector2 size = get_sprite_size(sprite);
		pos.x -= size.x * 0.5;
		pos.y -= size.y * 0.5;
		
		draw_image(sprite->image, pos, size, COLOR_WHITE);
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

	// :sound init
	fmod_init();
	#if CONFIGURATION == RELEASE
	play_sound("event:/bg_loop");
	#endif

	// :col
	col_exp = hex_to_rgba(0x7bd47aff);
	color_0 = hex_to_rgba(0x2a2d3aff);
	col_oxygen = hex_to_rgba(0xaad9e6ff);
	col_tether = col_oxygen;
	col_tether.a = 0.5;

	// sprite setup
	{
		sprites[0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/missing_tex.png"), get_heap_allocator()) };
		sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/player.png"), get_heap_allocator()) };
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

	// :arch data setup
	{
		archetype_data[ARCH_workbench].pretty_name = STR("Workbench");
		archetype_data[ARCH_furnace].pretty_name = STR("Furnace");
		archetype_data[ARCH_research_station].pretty_name = STR("Research Station");
		archetype_data[ARCH_teleporter1].pretty_name = STR("Teleporter");
		archetype_data[ARCH_tether].pretty_name = STR("Tether");
	}

	// :building resource setup
	{
		buildings[BUILDING_tether] = (BuildingData){
			.to_build=ARCH_tether,
			.icon=SPRITE_tether,
			.description=STR("Extends oxygen range"),
			.exp_cost=50,
			.ingredients_count=1,
			.ingredients={ {ITEM_copper_ingot, 2} }
		};

		buildings[BUILDING_furnace] = (BuildingData){
			.to_build=ARCH_furnace,
			.icon=SPRITE_furnace,
			.description=STR("Can burn stuff into something more useful."),
			.exp_cost=30,
			.ingredients_count=1,
			.ingredients={ {ITEM_rock, 20} }
		};

		buildings[BUILDING_workbench] = (BuildingData){
			.to_build=ARCH_workbench,
			.icon=SPRITE_workbench,
			.description=STR("Crafts things."),
			.ingredients_count=2,
			.ingredients={ {ITEM_pine_wood, 10}, {ITEM_fiber, 5} }
		};

		buildings[BUILDING_research_station] = (BuildingData){
			.to_build=ARCH_research_station,
			.icon=SPRITE_research_station,
			.description=STR("Researches recipes to unlock more buildings. This should wrap nicely at some point in the future lol..."),
			.ingredients_count=2,
			.ingredients={ {ITEM_pine_wood, 5}, {ITEM_rock, 1} }
		};
		buildings[BUILDING_teleporter1] = (BuildingData){
			.to_build=ARCH_teleporter1,
			.icon=SPRITE_teleporter1,
			.description=STR("A gateway to the next world."),
			.exp_cost=1000,
			.ingredients_count=2,
			.ingredients={ {ITEM_pine_wood, 100}, {ITEM_rock, 100} }
		};
	}

	// item data resource setup
	{
		// do defaults
		for (ItemID i = 1; i < ITEM_MAX; i++) {
			ItemData* data = &item_data[i];
			data->craft_length = 2.0;
		}

		// :item
		item_data[ITEM_o2_shard] = (ItemData){ .pretty_name=STR("Oxygen Shard"), .icon=SPRITE_o2_shard};
		item_data[ITEM_exp] = (ItemData){ .pretty_name=STR("Old Rock Thing"), .icon=SPRITE_exp};
		item_data[ITEM_rock] = (ItemData){ .pretty_name=STR("Rock"), .icon=SPRITE_item_rock };
		item_data[ITEM_pine_wood] = (ItemData){ .pretty_name=STR("Pine Wood"), .icon=SPRITE_item_pine_wood };
		item_data[ITEM_raw_copper] = (ItemData){ .pretty_name=STR("Raw Copper"), .icon=SPRITE_raw_copper };
		item_data[ITEM_fiber] = (ItemData){ .pretty_name=STR("Fiber"), .icon=SPRITE_fiber };
		item_data[ITEM_flint] = (ItemData){ .pretty_name=STR("Flint"), .icon=SPRITE_flint };

		item_data[ITEM_copper_ingot] = (ItemData){
			.pretty_name=STR("Copper Ingot"),
			.description=STR("Shiny"),
			.icon=SPRITE_copper_ingot,
			.craft_length=20,
			.for_structure=ARCH_furnace,
			.crafting_recipe_count=2,
			.crafting_recipe={
				{ITEM_raw_copper, 3},
				{ITEM_coal, 3},
			},
		};

		item_data[ITEM_coal] = (ItemData){
			.pretty_name=STR("Coal"),
			.description=STR("FUEEEEEEL"),
			.icon=SPRITE_coal,
			.craft_length=5,
			.for_structure=ARCH_furnace,
			.crafting_recipe_count=1,
			.crafting_recipe={
				{ITEM_pine_wood, 1},
			},
		};

		// NOTE
		// maybe we move some of these guys into the first round of item researching??
		//
		item_data[ITEM_flint_axe] = (ItemData){
			.pretty_name=STR("Flint Axe"),
			.description=STR("+1 damage to trees\nThis is also something that should wrap really nicely. Yay look at that sexy text box wrap. Isn't that just beautiful."),
			.icon=SPRITE_flint_axe,
			.craft_length=10,
			.extra_axe_dmg=1,
			.for_structure=ARCH_workbench,
			.crafting_recipe_count=3,
			.crafting_recipe={
				{ITEM_pine_wood, 10},
				{ITEM_flint, 10},
				{ITEM_fiber, 10},
			},
		};

		item_data[ITEM_flint_pickaxe] = (ItemData){
			.pretty_name=STR("Flint Pickaxe"),
			.description=STR("+1 damage to rocks"), // todo - make this functional from the extra dmg state
			.icon=SPRITE_flint_pickaxe,
			.craft_length=10,
			.extra_pickaxe_dmg=1,
			.for_structure=ARCH_workbench,
			.crafting_recipe_count=3,
			.crafting_recipe={
				{ITEM_pine_wood, 10},
				{ITEM_flint, 10},
				{ITEM_fiber, 10},
			},
		};

		item_data[ITEM_flint_scythe] = (ItemData){
			.pretty_name=STR("Flint Scythe"),
			.description=STR("+1 damage to grass"),
			.icon=SPRITE_flint_scythe,
			.craft_length=10,
			.extra_sickle_dmg=1,
			.for_structure=ARCH_workbench,
			.crafting_recipe_count=3,
			.crafting_recipe={
				{ITEM_pine_wood, 10},
				{ITEM_flint, 10},
				{ITEM_fiber, 10},
			},
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

		if (world->ux_state == UX_oxygenerator) {
			world_frame.hover_consumed = true;
			world_frame.show_inventory = true;
		}

		do_ui_stuff();

		// world update
		{
			world->time_elapsed += delta_t;
		}

		TileEntityCache* cache = create_tile_entity_pair_cache();

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
					tdata.entity_at_tile = entity_at_tile(cache, tdata.tile);
					growing_array_add((void**)&tiles, &tdata);
				}
			}

			tiles_for_biome[biome] = tiles;
		}

		// :spawn_res in world
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
					Vector2 spawn_pos = v2_tile_pos_to_world_pos(spawn_tile);

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

		// select entity
		if (!world_frame.hover_consumed)
		{
			// log("%f, %f", mouse_pos_world.x, mouse_pos_world.y);
			// draw_text(font, sprint(temp, STR("%f %f"), mouse_pos_world.x, mouse_pos_world.y), font_height, mouse_pos_world, v2(0.1, 0.1), COLOR_RED);

			float smallest_dist = INFINITY;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				bool has_interaction = en->destroyable_world_item
					|| en->workbench_thing
					|| en->arch == ARCH_research_station
					|| en->arch == ARCH_oxygenerator
					|| en->right_click_remove;
				// add extra :interact cases here ^^

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

		// :tile rendering
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
					if (biome == BIOME_forest) {
						col = v4_lerp(col, COLOR_BLUE, 0.1);
					}
					float x_pos = x * tile_width;
					float y_pos = y * tile_width;
					draw_rect(v2(x_pos + tile_width * -0.5, y_pos + tile_width * -0.5), v2(tile_width, tile_width), col);
				}
			}

			// draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
		}
		
		// :update entities
		Entity* player = get_player();
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {
				// switch (en->arch) {
					// ...
				// }

				// :workbench update
				if (en->workbench_thing) {
					if (en->current_crafting_item) {
						float length = get_item_data(en->current_crafting_item).craft_length;
						if (en->crafting_end_time == 0) {
							en->crafting_end_time = now() + length;
						}

						float alpha = alpha_from_end_time(en->crafting_end_time, length);
						// log("%f", alpha);

						if (has_reached_end_time(en->crafting_end_time)) {
							// craft thing
							{
								Entity* item = entity_create();
								setup_item(item, en->current_crafting_item);
								item->pos = en->pos;
							}
							en->current_crafting_amount -= 1;
							en->crafting_end_time = 0;
							assert(en->current_crafting_amount >= 0, "fuckie wuckie");
							if (en->current_crafting_amount == 0) {
								en->current_crafting_item = 0;
							}
						}
					}
				}

				// :oxygenerator update
				if (en->arch == ARCH_oxygenerator) {

					if (has_reached_end_time(en->fuel_expire_end_time)) {
						en->fuel_expire_end_time = 0;
					}

					// check for fuel expire.
					if (en->fuel_expire_end_time == 0 && en->input0.id) {
						// consume fuel
						en->input0.amount -= 1;
						if (en->input0.amount <= 0) {
							en->input0 = (ItemAmount){0};
						}
						en->fuel_expire_end_time = now() + o2_fuel_length;
					}

					if (en->fuel_expire_end_time != 0) {
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

		// :physics update
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (!en->is_valid || !en->has_physics) {
				continue;
			}

			// https://guide.handmadehero.org/code/day043
			
			// "friction"
			if (!en->disable_friction) {
				en->acceleration = v2_sub(en->acceleration, v2_mulf(en->velocity, en->friction));
			}

			// integrate
			en->velocity = v2_add(en->velocity, v2_mulf(en->acceleration, delta_t));
			Vector2 next_pos = v2_add(en->pos, v2_mulf(en->velocity, delta_t));
			en->acceleration = (Vector2){0};

			en->pos = next_pos;
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
							draw_line(v2_add(connected_tether->pos, tether_connection_offset), v2_add(current->pos, tether_connection_offset), 1.0f, col_tether);
						}
					}
				}
			}
		}

		// :player specific caveman update
		{
			Entity* closest_tether = 0;
			float closest_dist = INFINITY;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* tether = &world->entities[i];
				if (tether->is_valid && tether->is_oxygen_tether && tether->frame.is_powered) {
					float dist = v2_dist(tether->pos, player->pos);
					if (dist < tether_connection_radius) {
						if (!closest_tether || dist < closest_dist) {
							closest_tether = tether;
							closest_dist = dist;
						}
					}
				}
			}

			bool is_losing_o2 = closest_tether == null;
			app_frame.losing_o2 = is_losing_o2;

			if (!is_losing_o2) {
				player->oxygen_deplete_end_time = 0; // reset so it's a clean timer

				draw_line(v2_add(closest_tether->pos, tether_connection_offset), player->pos, 1.0f, col_tether);
				if (player->oxygen_regen_end_time == 0) {
					player->oxygen_regen_end_time = now() + oxygen_regen_tick_length;
				}
				if (has_reached_end_time(player->oxygen_regen_end_time)) {
					player->oxygen_regen_end_time = 0;
					player->oxygen += 1;
				}
			} else {
				if (player->oxygen_deplete_end_time == 0) {
					player->oxygen_deplete_end_time = now() + oxygen_deplete_tick_length;
				}
				if (has_reached_end_time(player->oxygen_deplete_end_time)) {
					player->oxygen_deplete_end_time = 0;
					player->oxygen -= 1;
				}
			}

			player->oxygen = clamp(player->oxygen, 0, get_max_oxygen());

			{
				local_persist FMOD_STUDIO_EVENTINSTANCE* o2_riser = 0;

				if (is_losing_o2 && !last_app_frame.losing_o2) {
					// just left tether
					o2_riser = play_sound("event:/o2_riser");
				}

				if (!is_losing_o2 && last_app_frame.losing_o2) {
					// just got back to tether
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

		// :selection stuff
		if (is_player_alive())
		{
			Entity* selected_en = world_frame.selected_entity;

			// :click destroy
			if (selected_en && selected_en->destroyable_world_item) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);

					play_sound("event:/hit_generic");
					selected_en->white_flash_current_alpha = 1.0;
					camera_shake(0.1);
					particle_emit(get_mouse_pos_in_world_space(), PFX_hit);

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

						ItemID *drops;
						// purposefully making the reserve 2 items, to prove the resizing works, and that you don't have to worry about the size of the array.
						growing_array_init_reserve((void**)&drops, sizeof(ItemID), 2, get_temporary_allocator());

						// drops from entity data
						for (int i = 0; i < selected_en->drops_count; i++) {
							ItemAmount drop = selected_en->drops[i];
							for (int j = 0; j < drop.amount; j++) {
								growing_array_add((void**)&drops, &drop.id);
							}
						}

						// create all the item drops
						for (int i = 0; i < growing_array_get_valid_count(drops); i++) {
							ItemID drop_id = drops[i];
							Entity* drop = entity_create();
							setup_item(drop, drop_id);
							drop->pos = selected_en->pos;
							drop->pos = v2_add(drop->pos, v2(get_random_float32_in_range(-2, 2), get_random_float32_in_range(-2, 2)));
							drop->pick_up_cooldown_end_time = now() + get_random_float32_in_range(0.1, 0.3);
						}

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

			// :interact workbench
			if (selected_en && selected_en->workbench_thing) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					world->ux_state = UX_workbench;
					world->interacting_with_entity = selected_en;
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

			// interact oxygenerator
			if (selected_en && selected_en->arch == ARCH_oxygenerator) {
				if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					world->ux_state = UX_oxygenerator;
					world->interacting_with_entity = selected_en;
				}
			}

			// right click remove
			if (selected_en && selected_en->right_click_remove) {
				if (is_key_just_pressed(MOUSE_BUTTON_RIGHT)) {
					consume_key_just_pressed(MOUSE_BUTTON_RIGHT);
					for (int i = 1; i < BUILDING_MAX; i++) {
						BuildingData data = buildings[i];
						if (data.to_build == selected_en->arch) {
							for (int j = 0; j < data.ingredients_count; j++) {
								ItemAmount amt = data.ingredients[j];
								Entity* drop = entity_create();
								setup_item(drop, amt.id);
								drop->item_amount = amt.amount;
								drop->pos = selected_en->pos;
							}
							break;
						}
					}
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
						// @volatile with entity placement
						xform         = m4_translate(xform, v3(0, tile_width * -0.5, 0));
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(get_sprite_size(sprite).x * -0.5, 0.0, 0));

						Vector4 col = COLOR_WHITE;
						if (world_frame.selected_entity == en) {
							col = COLOR_RED;
						}

						if (en->white_flash_current_alpha != 0) {
							animate_f32_to_target(&en->white_flash_current_alpha, 0, delta_t, 20.0f);
						}

						Draw_Quad* q = draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);
						set_col_override(q, v4(1, 1, 1, en->white_flash_current_alpha));

						// debug pos 
						// draw_text(font, sprint(temp, STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);

						break;
					}
				}

				// :oxygenerator render
				if (en->arch == ARCH_oxygenerator) {
					Vector2 size = {1, 5};
					Vector2 draw_pos = en->pos;
					draw_pos.x -= 0.5;
					draw_pos.y -= 3;

					size.y = size.y * (1.0-alpha_from_end_time(en->fuel_expire_end_time, o2_fuel_length));

					draw_rect(draw_pos, v2(size.x, size.y), col_oxygen);
				}

				// health bar
				if (en->health && en->health < en->max_health) {
					Vector2 size = {6, 1};
					Vector2 draw_pos = en->pos;
					draw_pos.x -= size.x * 0.5;
					draw_pos.y -= 6.0;

					draw_rect(draw_pos, size, COLOR_BLACK);

					float target_alpha = (float)en->health / (float)en->max_health;

					if (en->health_bar_current_alpha == 0.0) {
						en->health_bar_current_alpha = 1.0;
					}
					animate_f32_to_target(&en->health_bar_current_alpha, target_alpha, delta_t, 30.0f);

					draw_rect(draw_pos, v2(size.x * en->health_bar_current_alpha, size.y), COLOR_WHITE);
				}

				// :tether draw blue thingy
				if (en->arch != ARCH_oxygenerator && en->frame.is_powered) {
					draw_rect(v2_add(en->pos, v2(-1, 3)), v2(2, 2), col_oxygen);
				}

				// :workbench render
				// craft progress thing
				if (en->current_crafting_item && en->workbench_thing) {
					float radius = 4.0; 

					{
						Matrix4 xform = m4_identity;
						xform = m4_translate(xform, v3(en->pos.x, en->pos.y + 14.0, 0));
						xform = m4_translate(xform, v3(-radius, -radius, 0));
						draw_circle_xform(xform, v2(radius*2, radius*2), COLOR_WHITE);
					}

					ItemData craft_item_data = get_item_data(en->current_crafting_item);

					float alpha = alpha_from_end_time(en->crafting_end_time, craft_item_data.craft_length);

					{
						Matrix4 xform = m4_identity;
						xform = m4_translate(xform, v3(en->pos.x, en->pos.y + 14.0, 0));
						xform = m4_scale(xform, v3(alpha, alpha, 1.0));
						xform = m4_translate(xform, v3(-radius, -radius, 0));
						draw_circle_xform(xform, v2(radius*2, radius*2), COLOR_BLACK);
					}
				}
			}
		}

		// in-game :ui
		{
			// randy: I'm trying out making this UI be more diagetic and in the world.
			// that way we can just communicate the inputs/outputs and have the bare minimum info.
			// Instead of popping up a big ugly UI box.
			// I think it's roughly the same amount of work to do it this way, if proven effective.
			// :oxygenerator ui
			if (world->ux_state == UX_oxygenerator && world->interacting_with_entity) {

				Entity* en = world->interacting_with_entity;
				float x0 = en->pos.x;
				float y0 = en->pos.y;
				y0 += 6.f;

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
					set_col_override(quad, v4(0,0,0, 0.8));
				}

				if (do_tooltip) {
					item_tooltip(en->input0);
				}
			}
		}

		// render player
		if (is_player_alive())
		{
			Entity* en = get_player();
			Sprite* sprite = get_sprite(en->sprite_id);
			Matrix4 xform = m4_scalar(1.0);
			xform         = m4_translate(xform, v3(0, tile_width * -0.5, 0));
			xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
			xform         = m4_translate(xform, v3(get_sprite_size(sprite).x * -0.5, 0.0, 0));

			Vector4 col = COLOR_WHITE;
			draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);
		}

		// player movement
		if (is_player_alive())
		{
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
			player->pos = v2_add(player->pos, v2_mulf(input_axis, 70.0 * delta_t));
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
				float alpha = (float)player->oxygen / (float)get_max_oxygen();
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
		#if defined(DEBUG)
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