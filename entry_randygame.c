// The entry function is at the bottom of this file.
// Go there if you want to read this codebase.
// C needs declarations to be ordered, and header files are a waste of time. So reading this top to bottom isn't recommended.

#include "range.c"
#include "config.c"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

inline float v2_dist(Vector2 a, Vector2 b) {
  return fabsf(v2_length(v2_sub(a, b)));
}

#define m4_identity m4_make_scale(v3(1, 1, 1))

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
		case PIVOT_center_center: pivot_mul = v2(0.5, 0.5); break;
		case PIVOT_center_left: pivot_mul = v2(0.0, 0.5); break;
		case PIVOT_top_center: pivot_mul = v2(0.5, 1.0); break;
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

//

// 0 -> 1
float sin_breathe(float time, float rate) {
	return (sin(time * rate) + 1.0) / 2.0;
}

bool v4_equals(Vector4 a, Vector4 b) {
 return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

bool almost_equals(float a, float b, float epsilon) {
 return fabs(a - b) <= epsilon;
}

bool animate_f32_to_target(float* value, float target, float delta_t, float rate) {
	*value += (target - *value) * (1.0 - pow(2.0f, -rate * delta_t));
	if (almost_equals(*value, target, 0.001f))
	{
		*value = target;
		return true; // reached
	}
	return false;
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

// :tweaks
float tether_connection_radius = 50.0;
float oxygen_regen_tick_length = 0.01;
float oxygen_deplete_tick_length = 0.1;
float teleporter_radius = 8.0f;
Vector4 bg_box_col = {0, 0, 0, 0.5};
const int tile_width = 8;
const float world_half_length = tile_width * 10;
const float entity_selection_radius = 16.0f;
const float player_pickup_radius = 10.0f;
const int grass_health = 3;
const int flint_depo_health = 3;
const int exp_vein_health = 3;
const int ore1_health = 3;
const int rock_health = 3;
const int tree_health = 3;
const s32 layer_ui = 20;
const s32 layer_world = 10;

// :global app stuff
float64 delta_t;
Gfx_Font* font;
u32 font_height = 48;
u32 font_height_body = 36;

int world_pos_to_tile_pos(float world_pos) {
	return roundf(world_pos / (float)tile_width);
}

float tile_pos_to_world_pos(int tile_pos) {
	return ((float)tile_pos * (float)tile_width);
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
	SPRITE_ore1,
	SPRITE_ore1_item,
	SPRITE_fiber,
	SPRITE_flint,
	SPRITE_flint_axe,
	SPRITE_flint_depo,
	SPRITE_flint_pickaxe,
	SPRITE_flint_scythe,
	SPRITE_grass,
	SPRITE_coal,
	SPRITE_core_tether,
	SPRITE_tether,
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
	ITEM_ore1,
	ITEM_fiber,
	ITEM_flint,
	ITEM_flint_axe,
	ITEM_flint_pickaxe,
	ITEM_flint_scythe,
	ITEM_coal,
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
	ARCH_ore1 = 11,
	ARCH_flint_depo = 12,
	ARCH_grass = 13,
	ARCH_core_tether = 14,
	ARCH_tether = 15,
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

// :dimension
typedef enum DimensionID {
	DIM_nil,
	DIM_first, // very inventive names!
	DIM_second,
	DIM_third,
	DIM_fourth,
	DIM_MAX,
} DimensionID;
typedef struct DimensionData {
	string pretty_name;
} DimensionData;
DimensionData dimension_data[DIM_MAX] = {0};

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
	bool destroyable_world_item;
	bool is_item;
	bool workbench_thing;
	ItemID current_crafting_item;
	int current_crafting_amount;
	float64 crafting_end_time;
	DimensionID current_dimension;
	DimensionID teleport_to_dimension_when_near;
	float64 tp_cooldown_end_time;
	ItemAmount drops[4];
	int drops_count;
	DamageType dmg_type;
	ItemID selected_crafting_item;
	int oxygen;
	float64 oxygen_deplete_end_time;
	float64 oxygen_regen_end_time;
	bool is_oxygen_tether;

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
	int pct_per_research_exp; // this jank will get replaced with a recipe one day
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

typedef struct WorldResourceData {
	DimensionID dim_id;
	ArchetypeID arch_id;
	float spawn_interval;
	int max_count;
} WorldResourceData;
// NOTE - trying out a new pattern here. That way we don't have to keep writing up enums to index into these guys. If we need dynamic runtime data, just make an array with the count of this array and have it essentially share the index. Like what I've done below in the world state.
WorldResourceData world_resources[] = {
	{ DIM_first, ARCH_rock, 2.f, 4 },
	{ DIM_first, ARCH_tree, 1.f, 10 },
	// { DIM_first, ARCH_exp_vein, 3.f, 2 },
	{ DIM_first, ARCH_flint_depo, 3.f, 2 },
	{ DIM_first, ARCH_grass, 3.f, 10 },

	{ DIM_second, ARCH_exp_vein, 2.5f, 10 },
	{ DIM_second, ARCH_ore1, 2.5f, 10 },
	// :spawn_res system
};

// :biome system
typedef enum BiomeID {
	BIOME_nil,
	BIOME_void,
	BIOME_test,
	BIOME_yeet,
	BIOME_MAX,
} BiomeID;

typedef struct Map {
	int width;
	int height;
	BiomeID* tiles;
} Map;

Map world_maps[DIM_MAX] = {0};
void init_biome_maps() {

	Map* map = &world_maps[DIM_first];

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

	map->width = width;
	map->height = height;
	map->tiles = alloc(get_heap_allocator(), width * height * sizeof(BiomeID));

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
			map->tiles[index] = BIOME_test;
		}
	}
}

BiomeID biome_at_tile(DimensionID dim, int x, int y) {
	Map map = world_maps[dim];
	BiomeID biome = 0;
	int x_index = x + floor((float)map.width * 0.5);
	int y_index = y + floor((float)map.height * 0.5);
	if (x_index < map.width && x_index >= 0 && y_index < map.height && y_index >= 0) {
		biome = map.tiles[y_index * map.width + x_index];
	}
	return biome;
}

typedef struct WorldDimension {
	float64 resource_next_spawn_end_time[ARRAY_COUNT(world_resources)];
} WorldDimension;

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
	WorldDimension dimensions[DIM_MAX];
	// todo - figure out if we can legit just keep this as a pointer or not lol
	Entity* core_tether;
	// :world :state
} World;
World* world = 0;

typedef struct WorldFrame {
	Entity* selected_entity;
	Matrix4 world_proj;
	Matrix4 world_view;
	bool hover_consumed;
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
	return 100;
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

	// put in player's dimension
	Entity* player = get_player();
	if (player) {
		entity_found->current_dimension = player->current_dimension;
	} else {
		entity_found->current_dimension = DIM_first;
	}

	return entity_found;
}

void entity_destroy(Entity* entity) {
	memset(entity, 0, sizeof(Entity));
}

// :setup things

void setup_tether(Entity* en) {
	en->arch = ARCH_tether;
	en->sprite_id = SPRITE_tether;
	en->is_oxygen_tether = true;
}

void setup_core_tether(Entity* en) {
	en->arch = ARCH_core_tether;
	en->sprite_id = SPRITE_core_tether;
	en->is_oxygen_tether = true;
}

void setup_grass(Entity* en) {
	en->arch = ARCH_grass;
	en->sprite_id = SPRITE_grass;
	en->health = grass_health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_fiber, .amount=get_random_int_in_range(1, 2)};
}

void setup_flint_depo(Entity* en) {
	en->arch = ARCH_flint_depo;
	en->sprite_id = SPRITE_flint_depo;
	en->health = flint_depo_health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_flint, .amount=get_random_int_in_range(2, 3)};
}

void setup_ore1(Entity* en) {
	en->arch = ARCH_ore1;
	en->sprite_id = SPRITE_ore1;
	en->health = ore1_health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_ore1, .amount=get_random_int_in_range(2, 3)};
}

void setup_teleporter1(Entity* en) {
	en->arch = ARCH_teleporter1;
	en->sprite_id = SPRITE_teleporter1;
	en->teleport_to_dimension_when_near = DIM_second;
}

void setup_exp_vein(Entity* en) {
	en->arch = ARCH_exp_vein;
	en->sprite_id = SPRITE_exp_vein;
	en->health = exp_vein_health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_exp, .amount=get_random_int_in_range(2, 3)};
}

void setup_furnace(Entity* en) {
	en->arch = ARCH_furnace;
	en->sprite_id = SPRITE_furnace;
	en->workbench_thing = true;
}

void setup_workbench(Entity* en) {
	en->arch = ARCH_workbench;
	en->sprite_id = SPRITE_workbench;
	en->workbench_thing = true;
}

void setup_research_station(Entity* en) {
	en->arch = ARCH_research_station;
	en->sprite_id = SPRITE_research_station;
}

void setup_player(Entity* en) {
	en->arch = ARCH_player;
	en->sprite_id = SPRITE_player;
	en->health = 1;
	en->oxygen = get_max_oxygen();
}

void setup_rock(Entity* en) {
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock0;
	en->health = rock_health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_rock, .amount=get_random_int_in_range(2, 3)};
}

void setup_tree(Entity* en) {
	en->arch = ARCH_tree;
	en->sprite_id = SPRITE_tree1;
	// en->sprite_id = SPRITE_tree1;
	en->health = tree_health;
	en->destroyable_world_item = true;
	en->drops_count = 1;
	en->drops[0] = (ItemAmount){.id=ITEM_pine_wood, .amount=get_random_int_in_range(2, 3)};
	en->dmg_type = DMG_axe;
}

void setup_item(Entity* en, ItemID item_id) {
	en->arch = ARCH_item;
	en->sprite_id = get_sprite_id_from_item(item_id);
	en->is_item = true;
	en->item_id = item_id;
	en->item_amount = 1;
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
		case ARCH_ore1: setup_ore1(en); break;
		case ARCH_flint_depo: setup_flint_depo(en); break;
		case ARCH_grass: setup_grass(en); break;
		case ARCH_core_tether: setup_core_tether(en); break;
		case ARCH_tether: setup_tether(en); break;
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
	player_en->current_dimension = DIM_first;

	Entity* en = entity_create();
	setup_core_tether(en);
	world->core_tether = en;

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

		en = entity_create();
		setup_furnace(en);
		en->pos.y = 20.0;

		en = entity_create();
		setup_research_station(en);
		en->pos.x = -20.0;
	}
	#endif
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

bool is_in_player_dimension(Entity* en) {
	Entity* player = get_player();
	// we'll do some stuff later on where this can be in multiple dimensions, just by && this bad boy
	bool portal_at_destination = en->teleport_to_dimension_when_near && en->teleport_to_dimension_when_near == player->current_dimension;
	return en->current_dimension == player->current_dimension || portal_at_destination;
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

	// :respawn ui
	if (world->ux_state == UX_respawn) {
		u32 title_font_height = 128;
		u32 subtitle_font_height = 64;

		float x0 = screen_width * 0.5;
		float y0 = screen_height * 0.6666;
		draw_text_with_pivot(font, STR("DED"), title_font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_bottom_center);

		y0 -= 5.0;
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
		animate_f32_to_target(&world->inventory_alpha, world->inventory_alpha_target, delta_t, 15.0);
		bool is_inventory_enabled = world->inventory_alpha_target == 1.0;
		if (world->inventory_alpha_target != 0.0)
		{
			// TODO - some opacity thing here.
			float y_pos = 70.0;

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
				Matrix4 xform = m4_identity;
				xform = m4_translate(xform, v3(x_start_pos, y_pos, 0.0));
				draw_rect_xform(xform, v2(entire_thing_width_idk, icon_width), bg_box_col);
			}

			int slot_index = 0;
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
						draw_rect_xform(xform, box_size, bg_box_col);

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

		Vector2 section_size = v2(50.0, 70.0);
		float gap_between_panels = 10.0;
		float text_height_pad = 4.0;

		float ui_width_thing = section_size.x * 2.0 + gap_between_panels;

		float x0 = screen_width * 0.5 - ui_width_thing * 0.5;
		float y0 = screen_height * 0.5 - section_size.y * 0.5;
		float x_left_pane_start = x0;
		float y_bottom = y0;
		float y_top = y0 + section_size.y;

		// left pane
		{
			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x0, y0, 0));
			draw_rect_xform(xform, section_size, bg_col);

			y0 = y_top;

			// title
			{
				string title = STR("Research Station");
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

			Vector2 item_icon_size = v2(8, 8);

			y0 -= item_icon_size.y;

			// draw research list
			// TODO - make this an icon list like the crafting. That way it's more close to the final design of the tech tree
			for (int i = 1; i < BUILDING_MAX; i++) {
				UnlockState unlock_state = world->building_unlocks[i];
				BuildingData building_data = get_building_data(i);
				if (is_fully_unlocked(unlock_state)) {
					continue;
				}

				Vector2 element_size = v2(section_size.x * 0.8, 6.0);

				x0 = x_left_pane_start + (section_size.x - element_size.x) * 0.5;

				// bg box thing
				Vector2 box_start = v2(x0, y0);
				Range2f box = range2f_make_bottom_left(box_start, element_size);
				if (range2f_contains(box, get_mouse_pos_in_world_space())) {
					// todo - hover ux
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						world->selected_research_thing = i;
					}
				}
				draw_rect(box_start, element_size, fill_col);

				// get icon size
				float item_icon_length = element_size.y;

				// get text size
				string txt = get_archetype_pretty_name(building_data.to_build);
				Vector2 txt_size;
				Vector2 txt_offset_for_center;
				{
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					txt_size = metrics.visual_size;
					txt_offset_for_center = metrics.visual_pos_min;
					txt_offset_for_center = v2_sub(txt_offset_for_center, v2_mul(metrics.visual_size, v2(0, 0.5)));
				}

				float pad_between_elements = 2.0;
				float total_width = item_icon_length + txt_size.x + pad_between_elements;

				// draw icon
				{
					x0 = (box_start.x + element_size.x * 0.5) - total_width * 0.5;
					y0 = box_start.y;

					Range2f box = range2f_make_bottom_left(v2(x0, y0), v2(item_icon_length, item_icon_length));
					draw_sprite_in_rect(building_data.icon, box, COLOR_WHITE, 0.2);
				}
				x0 += item_icon_length;
				x0 += pad_between_elements;
				y0 = box_start.y; // reset the Y for the next txt element

				// draw txt
				{
					Vector2 draw_pos = v2(x0, y0 + element_size.y * 0.5);
					draw_pos = v2_add(draw_pos, txt_offset_for_center);
					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
				}

				y0 -= element_size.y;
				y0 -= 2.0f; // padding @cleanup
			}
		}
		
		y0 = y_bottom;
		x0 = x_left_pane_start;
		x0 += section_size.x;
		x0 += gap_between_panels;


		// right pane
		float x_right_pane_start = x0;
		{
			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x0, y0, 0));
			draw_rect_xform(xform, section_size, bg_col);

			if (world->selected_research_thing) {
				BuildingData building_data = get_building_data(world->selected_research_thing);
				UnlockState* unlock_data = &world->building_unlocks[world->selected_research_thing];

				// title
				y0 += section_size.y;
				{
					string title = get_archetype_pretty_name(get_building_data(world->selected_research_thing).to_build);
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

				// research % bar
				{
					Vector2 size = v2(section_size.x * 0.8, 4.0);
					float research_alpha = (float)unlock_data->research_progress / 100.0f;

					y0 -= size.y;
					y0 -= 4.0; // element padding
					x0 += (section_size.x - size.x) * 0.5; // center horizontally

					// bg
					draw_rect(v2(x0, y0), size, fill_col);

					// fill
					draw_rect(v2(x0, y0), v2(size.x * research_alpha, size.y), accent_col);

					string txt = tprint("%i%%", unlock_data->research_progress);

					x0 = x_right_pane_start;
					x0 += section_size.x * 0.5;
					y0 -= 4.0; // arbitrary

					draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, COLOR_WHITE, PIVOT_center_center);
				}

				// todo - put this into a research_recipe array so we can do multiple items for research.
				bool has_enough_for_research = world->inventory_items[ITEM_exp].amount > 0;

				// research button
				{
					Vector2 size = v2(section_size.x * 0.8, 6.0);

					x0 = x_right_pane_start + (section_size.x - size.x) * 0.5;
					y0 = y_bottom;
					y0 += 5.0f; // padding from bottom @cleanup

					Range2f btn_range = range2f_make_bottom_left(v2(x0, y0), size);
					Vector4 col = fill_col;
					if (has_enough_for_research && range2f_contains(btn_range, get_mouse_pos_in_world_space())) {
						col = COLOR_RED;
						world_frame.hover_consumed = true;
						// :research action
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							unlock_data->research_progress += building_data.pct_per_research_exp;
							world->inventory_items[ITEM_exp].amount -= 1;
							assert(world->inventory_items[ITEM_exp].amount >= 0, "pre-check failed.");
							if (unlock_data->research_progress >= 100) {
								unlock_data->research_progress = 100;
								// todo - epic feeback
								world->selected_research_thing = 0;
								world->ux_state = 0;
							}
						}
					}
					draw_rect(v2(x0, y0), size, col);

					string txt = STR("RESEARCH");
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					Vector2 draw_pos = v2(x0 + size.x * 0.5, y0 + size.y * 0.5);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0.5, 0.5)));

					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

					y0 += size.y;
				}

				y0 += 6.0f; // arbitrary spacing

				// material icon list
				{
					// EXP, x1 (red if out)
					x0 = x_right_pane_start + section_size.x * 0.5;

					// icon
					{
						float item_icon_length = 6.0;
						Range2f box = range2f_make_center_right(v2(x0, y0), v2(item_icon_length, item_icon_length));
						draw_sprite_in_rect(SPRITE_exp, box, COLOR_WHITE, 0.4);
					}

					Vector4 col = COLOR_WHITE;
					if (!has_enough_for_research) {
						col = COLOR_RED;
					}

					string txt = tprint("x1");
					draw_text_with_pivot(font, txt, font_height, v2(x0, y0), text_scale, col, PIVOT_center_left);
				}


			} else {
				// select item first text
				y0 += section_size.y * 0.5;
				{
					string title = STR("Select Recipe");
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
	window.width = 1280;
	window.height = 720;
	window.x = 200;
	window.y = 200;
	window.clear_color = COLOR_BLACK;

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	// :col
	color_0 = hex_to_rgba(0x2a2d3aff);
	col_oxygen = hex_to_rgba(0xaad9e6ff);
	col_tether = col_oxygen;
	col_tether.a = 0.5;

	// sprite setup
	{
		sprites[0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/missing_tex.png"), get_heap_allocator()) };
		sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/player.png"), get_heap_allocator()) };
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
		sprites[SPRITE_teleporter1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/teleporter1.png"), get_heap_allocator()) };
		sprites[SPRITE_ore1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/ore1.png"), get_heap_allocator()) };
		sprites[SPRITE_ore1_item] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/ore1_item.png"), get_heap_allocator()) };
		sprites[SPRITE_fiber] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/fiber.png"), get_heap_allocator())};
		sprites[SPRITE_flint] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint.png"), get_heap_allocator())};
		sprites[SPRITE_flint_axe] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_axe.png"), get_heap_allocator())};
		sprites[SPRITE_flint_depo] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_depo.png"), get_heap_allocator())};
		sprites[SPRITE_flint_pickaxe] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_pickaxe.png"), get_heap_allocator())};
		sprites[SPRITE_flint_scythe] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/flint_scythe.png"), get_heap_allocator())};
		sprites[SPRITE_grass] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/grass.png"), get_heap_allocator())};
		sprites[SPRITE_coal] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/coal.png"), get_heap_allocator())};
		sprites[SPRITE_core_tether] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/core_tether.png"), get_heap_allocator())};
		sprites[SPRITE_tether] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/tether.png"), get_heap_allocator())};
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
			.pct_per_research_exp=10,
			.ingredients_count=1,
			.ingredients={ {ITEM_rock, 20} }
		};

		buildings[BUILDING_furnace] = (BuildingData){
			.to_build=ARCH_furnace,
			.icon=SPRITE_furnace,
			.description=STR("Can burn stuff into something more useful."),
			.pct_per_research_exp=10,
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
			.pct_per_research_exp=2,
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
		item_data[ITEM_exp] = (ItemData){ .pretty_name=STR("Knowledge Fragment"), .icon=SPRITE_exp};
		item_data[ITEM_rock] = (ItemData){ .pretty_name=STR("Rock"), .icon=SPRITE_item_rock };
		item_data[ITEM_pine_wood] = (ItemData){ .pretty_name=STR("Pine Wood"), .icon=SPRITE_item_pine_wood };
		item_data[ITEM_ore1] = (ItemData){ .pretty_name=STR("Ore Thingy"), .icon=SPRITE_ore1_item };
		item_data[ITEM_fiber] = (ItemData){ .pretty_name=STR("Fiber"), .icon=SPRITE_fiber };
		item_data[ITEM_flint] = (ItemData){ .pretty_name=STR("Flint"), .icon=SPRITE_flint };

		item_data[ITEM_coal] = (ItemData){
			.pretty_name=STR("Coal"),
			.description=STR("FUEEEEEEL"),
			.icon=SPRITE_coal,
			.craft_length=0.5,
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

	// :dimension data setup
	{
		dimension_data[DIM_first].pretty_name = STR("First");
		dimension_data[DIM_second].pretty_name = STR("Second");
	}

	init_biome_maps();

	// the :init zone

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

	float zoom = 5.3;
	Vector2 camera_pos = v2(0, 0);

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

		// find player lol
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid && en->arch == ARCH_player) {
				world_frame.player = en;
			}
		}

		// :frame update
		draw_frame.enable_z_sorting = true;

		world_frame.world_proj = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		// :camera
		{
			Vector2 target_pos = get_player()->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f);

			world_frame.world_view = m4_make_scale(v3(1.0, 1.0, 1.0));
			world_frame.world_view = m4_mul(world_frame.world_view, m4_make_translation(v3(camera_pos.x, camera_pos.y, 0)));
			world_frame.world_view = m4_mul(world_frame.world_view, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}
		set_world_space();
		push_z_layer(layer_world);

		Vector2 mouse_pos_world = get_mouse_pos_in_world_space();
		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y);

		do_ui_stuff();

		// :world update
		{
			world->time_elapsed += delta_t;
		}

		// :spawn_res in world
		{
			for (int i = 0; i < ARRAY_COUNT(world_resources); i++) {
				WorldResourceData data = world_resources[i];

				for (DimensionID dim_id = 1; dim_id < ARRAY_COUNT(world->dimensions); dim_id++) {
					WorldDimension* dim = &world->dimensions[dim_id];
					if (data.dim_id != dim_id) {
						// skip, the resource isn't for this dimension
						continue;
					}

					// grab current entity count
					int entity_count = 0;
					for (int j = 0; j < MAX_ENTITY_COUNT; j++) {
						Entity* en = &world->entities[j];
						if (en->is_valid && en->current_dimension == dim_id && en->arch == data.arch_id) {
							entity_count += 1;
						}
					}

					// note - this is a bit confusing to read. But it's this way to only start the timer after we drop below the max entity count. So that when the player destroys an entity, it doesn't immediately respawn.
					if (entity_count >= data.max_count) {
						dim->resource_next_spawn_end_time[i] = 0.0;
					}
					if (dim->resource_next_spawn_end_time[i] == 0.0
					&& entity_count < data.max_count) {
						dim->resource_next_spawn_end_time[i] = now() + data.spawn_interval;
					}

					bool init_worldgen_hack = world->time_elapsed < 1.0;
					if (entity_count < data.max_count
					&& (has_reached_end_time(dim->resource_next_spawn_end_time[i]) || init_worldgen_hack)) {
						Entity* en = entity_create();
						en->current_dimension = dim_id;
						entity_setup(en, data.arch_id);
						en->pos = v2(get_random_float32_in_range(-world_half_length, world_half_length), get_random_float32_in_range(-world_half_length, world_half_length));
						en->pos = round_v2_to_tile(en->pos);
						dim->resource_next_spawn_end_time[i] = 0.0;
					}
				}
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
					|| en->arch == ARCH_research_station;
				// add extra :interact cases here ^^
				if (en->is_valid && is_in_player_dimension(en) && has_interaction) {
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

					BiomeID biome = biome_at_tile(get_player()->current_dimension, x, y);

					// checkerboard pattern
					Vector4 col = color_0;
					if ((x + (y % 2 == 0) ) % 2 == 0) {
						col.a = 0.9;
					}
					if (biome == BIOME_test) {
						col = v4_mul(col, COLOR_RED);
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

				// teleporter update
				if (has_reached_end_time(en->tp_cooldown_end_time) && is_in_player_dimension(en) && en->teleport_to_dimension_when_near) {
					bool in_teleport_radius = fabsf(v2_dist(en->pos, player->pos)) < teleporter_radius;
					if (in_teleport_radius) {
						if (player->current_dimension == en->teleport_to_dimension_when_near) {
							player->current_dimension = en->current_dimension;
						} else {
							player->current_dimension = en->teleport_to_dimension_when_near;
						}
						en->tp_cooldown_end_time = now() + 1.0;
					}
				}

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

				// pickup item
				if (is_player_alive() && is_in_player_dimension(en) && en->is_item) {
					// TODO - epic physics pickup like arcana
					if (fabsf(v2_dist(en->pos, get_player()->pos)) < player_pickup_radius) {
						world->inventory_items[en->item_id].amount += en->item_amount;
						entity_destroy(en);
					}
				}
			}
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
				growing_array_add((void**)&connection_stack, &world->core_tether);

				while (growing_array_get_valid_count(connection_stack)) {
					Entity* current = connection_stack[growing_array_get_valid_count(connection_stack)-1];
					growing_array_pop((void**)&connection_stack);

					for (int i = 0; i < growing_array_get_valid_count(current->frame.connected_to_tethers); i ++) {
						Entity* connected_tether = current->frame.connected_to_tethers[i];
						if (!connected_tether->frame.is_powered) {
							growing_array_add((void**)&connection_stack, &connected_tether);
							connected_tether->frame.is_powered = true;
							draw_line(connected_tether->pos, current->pos, 1.0f, col_tether);
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
				if (tether->is_valid && tether->is_oxygen_tether && (tether->frame.is_powered || tether->arch == ARCH_core_tether)) {
					float dist = v2_dist(tether->pos, player->pos);
					if (dist < tether_connection_radius) {
						if (!closest_tether || dist < closest_dist) {
							closest_tether = tether;
							closest_dist = dist;
						}
					}
				}
			}

			if (closest_tether) {
				draw_line(closest_tether->pos, player->pos, 1.0f, col_tether);
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
						// :drops

						ItemID *drops;
						// purposefully making the reserve 2 items, to prove the resizing works, and that you don't have to worry about the size of the array.
						growing_array_init_reserve((void**)&drops, sizeof(ItemID), 2, get_temporary_allocator());

						// add in exp
						if (pct_chance(0.5)) {
							growing_array_add((void**)&drops, &(ItemID){ITEM_exp});
						}
						
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
		}

		// :render entities
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid && is_in_player_dimension(en)) {

				switch (en->arch) {

					case ARCH_player: break;

					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						Matrix4 xform = m4_scalar(1.0);
						if (en->is_item) {
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

						draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);

						// debug pos 
						// draw_text(font, sprint(temp, STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);

						break;
					}
				}

				// :tether draw blue thingy
				if (en->is_oxygen_tether && en->frame.is_powered) {
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
			player->pos = v2_add(player->pos, v2_mulf(input_axis, 100.0 * delta_t));
		}

		// player :hud
		if (is_player_alive())
		{
			Entity* player = get_player();

			Vector2 size = {6, 1};
			Vector2 draw_pos = player->pos;
			draw_pos.x -= size.x * 0.5;
			draw_pos.y -= 6.0;

			draw_rect(draw_pos, size, COLOR_BLACK);

			float alpha = (float)player->oxygen / (float)get_max_oxygen();

			draw_rect(draw_pos, v2(size.x * alpha, size.y), col_oxygen);
		}

		gfx_update();
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

	return 0;
}