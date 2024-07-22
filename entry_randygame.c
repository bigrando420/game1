inline float v2_length(Vector2 a) {
    return sqrt(a.x * a.x + a.y * a.y);
}
// randy: is this something that's usually standard in math libraries or am I tripping?
inline float v2_dist(Vector2 a, Vector2 b) {
    return v2_length(v2_sub(a, b));
}

#define m4_identity m4_make_scale(v3(1, 1, 1))

Vector2 range2f_get_center(Range2f r) {
	return (Vector2) { (r.max.x - r.min.x) * 0.5 + r.min.x, (r.max.y - r.min.y) * 0.5 + r.min.y };
}

// ^^^ engine changes

// the scuff zone

Draw_Quad ndc_quad_to_screen_quad(Draw_Quad ndc_quad) {

	// NOTE: we're assuming these are the screen space matricies.
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.view;

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

// :tweaks
Vector4 bg_box_col = {0, 0, 0, 0.5};
const int tile_width = 8;
const float entity_selection_radius = 16.0f;
const float player_pickup_radius = 20.0f;
const int rock_health = 3;
const int tree_health = 3;

// :global app stuff
float64 delta_t;
Gfx_Font* font;
u32 font_height = 48;

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

typedef enum EntityArchetype {
	ARCH_nil = 0,
	ARCH_rock = 1,
	ARCH_tree = 2,
	ARCH_player = 3,
	ARCH_item_rock = 4,
	ARCH_item_pine_wood = 5,
	ARCH_furnace = 6,
	ARCH_workbench = 7,
	ARCH_MAX,
} EntityArchetype;

SpriteID get_sprite_id_from_archetype(EntityArchetype arch) {
	switch (arch) {
		case ARCH_item_pine_wood: return SPRITE_item_pine_wood; break;
		case ARCH_item_rock: return SPRITE_item_rock; break;
		default: return 0;
	}
}

string get_archetype_pretty_name(EntityArchetype arch) {
	switch (arch) {
		case ARCH_item_pine_wood: return STR("Pine Wood");
		case ARCH_item_rock: return STR("Rock");
		default: return STR("nil");
	}
}

typedef struct Entity {
	bool is_valid;
	EntityArchetype arch;
	Vector2 pos;
	bool render_sprite;
	SpriteID sprite_id;
	int health;
	bool destroyable_world_item;
	bool is_item;
} Entity;
// :entity
#define MAX_ENTITY_COUNT 1024

// :item
typedef struct ItemData {
	int amount;
} ItemData; 

// :building resource
// NOTE, a "resource" is a thing that we set up during startup, and is constant.
typedef enum BuildingID {
	BUILDING_nil,
	BUILDING_furnace,
	BUILDING_workbench,
	BUILDING_MAX,
} BuildingID;
typedef struct BuildingData {
	EntityArchetype to_build;
	SpriteID icon;
	// display name
	// cost
	// etc
} BuildingData;
BuildingData buildings[BUILDING_MAX];
BuildingData get_building_data(BuildingID id) {
	// note, this isn't a pointer, because this is constant resource data, we don't want to modify
	return buildings[id];
}

// :ux
typedef enum UXState {
	UX_nil,
	UX_inventory,
	UX_building,
	UX_place_mode,
} UXState;

// :world
typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];
	ItemData inventory_items[ARCH_MAX];
	UXState ux_state;
	float inventory_alpha;
	float inventory_alpha_target;
	float building_alpha;
	float building_alpha_target;
	BuildingID placing_building;
} World;
World* world = 0;

typedef struct WorldFrame {
	Entity* selected_entity;
	Matrix4 world_proj;
	Matrix4 world_view;
	// :frame state
} WorldFrame;
WorldFrame world_frame;

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

void setup_furnace(Entity* en) {
	en->arch = ARCH_furnace;
	en->sprite_id = SPRITE_furnace;
}

void setup_player(Entity* en) {
	en->arch = ARCH_player;
	en->sprite_id = SPRITE_player;
}

void setup_rock(Entity* en) {
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock0;
	en->health = rock_health;
	en->destroyable_world_item = true;
}

void setup_tree(Entity* en) {
	en->arch = ARCH_tree;
	en->sprite_id = SPRITE_tree1;
	// en->sprite_id = SPRITE_tree1;
	en->health = tree_health;
	en->destroyable_world_item = true;
}

void setup_item_pine_wood(Entity* en) {
	en->arch = ARCH_item_pine_wood;
	en->sprite_id = SPRITE_item_pine_wood;
	en->is_item = true;
}
void setup_item_rock(Entity* en) {
	en->arch = ARCH_item_rock;
	en->sprite_id = SPRITE_item_rock;
	en->is_item = true;
}

Vector2 get_mouse_pos_in_ndc() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.view;
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
	Matrix4 view = draw_frame.view;
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
	draw_frame.view = m4_scalar(1.0);
	draw_frame.projection = m4_make_orthographic_projection(0.0, screen_width, 0.0, screen_height, -1, 10);
}
void set_world_space() {
	draw_frame.projection = world_frame.world_proj;
	draw_frame.view = world_frame.world_view;
}

// :func dump
void do_ui_stuff() {
	set_screen_space();

	// "screen space"

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
				ItemData* item = &world->inventory_items[i];
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
			for (int i = 0; i < ARCH_MAX; i++) {
				ItemData* item = &world->inventory_items[i];
				if (item->amount > 0) {

					float slot_index_offset = slot_index * icon_width;

					Matrix4 xform = m4_scalar(1.0);
					xform = m4_translate(xform, v3(x_start_pos + slot_index_offset, y_pos, 0.0));

					Sprite* sprite = get_sprite(get_sprite_id_from_archetype(i));

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
						float scale_adjust = 0.3; //0.1 * sin_breathe(os_get_current_time_in_seconds(), 20.0);
						xform = m4_scale(xform, v3(1 + scale_adjust, 1 + scale_adjust, 1));
					}
					{
						// could also rotate ...
						// float adjust = PI32 * 2.0 * sin_breathe(os_get_current_time_in_seconds(), 1.0);
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
							string title = get_archetype_pretty_name(i);
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
							text = sprint(temp, text, item->amount);

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

			for (BuildingID i = 1; i < BUILDING_MAX; i++) {
				BuildingData* building = &buildings[i];

				Matrix4 xform = m4_identity;

				float x0 = (screen_width * 0.5) - (total_box_width * 0.5);
				x0 += (i-1) * icon_size;
				x0 += padding * i;
				xform = m4_translate(xform, v3(x0, 10, 0));

				Sprite* icon = get_sprite(building->icon);
				// todo - make a draw_sprite_xform, that auto sizes properly
				Draw_Quad* quad = draw_image_xform(icon->image, xform, v2(icon_size, icon_size), COLOR_WHITE);
				Range2f box = quad_to_range(*quad);

				if (range2f_contains(box, get_mouse_pos_in_ndc())) {
					// if hover, do tooltip, that follows the mouse around

					// if click, go into place mode
					if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
						consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						world->placing_building = i;
						world->ux_state = UX_place_mode;
					}
				}

				// draw_rect_xform(xform, v2(icon_size, icon_size), COLOR_WHITE);
			}
		}
	}

	// :placement mode
	// TODO - alpha animation for place mode
	if (world->ux_state == UX_place_mode)
	{
		Vector2 mouse_pos_world = get_mouse_pos_in_world_space();

		// TODO - put this into macro :)
		set_world_space();
		{
			// mouse_pos_world;
			BuildingData building = get_building_data(world->placing_building);
			Sprite* icon = get_sprite(building.icon);

			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(mouse_pos_world.x, mouse_pos_world.y, 0));
			xform = m4_translate(xform, v3(get_sprite_size(icon).x * -0.5, 0.0, 0));
			draw_image_xform(icon->image, xform, get_sprite_size(icon), COLOR_WHITE);
		}
		set_screen_space();
	}

	set_world_space();
}

int entry(int argc, char **argv) {
	window.title = STR("Randy's Game");
	window.width = 1280;
	window.height = 720;
	window.x = 200;
	window.y = 200;
	window.clear_color = hex_to_rgba(0x2a2d3aff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	sprites[0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/missing_tex.png"), get_heap_allocator()) };
	sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/player.png"), get_heap_allocator()) };
	sprites[SPRITE_tree0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree0.png"), get_heap_allocator()) };
	sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree1.png"), get_heap_allocator()) };
	sprites[SPRITE_rock0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/rock0.png"), get_heap_allocator()) };
	sprites[SPRITE_item_pine_wood] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_pine_wood.png"), get_heap_allocator()) };
	sprites[SPRITE_item_rock] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_rock.png"), get_heap_allocator()) };
	sprites[SPRITE_furnace] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/furnace.png"), get_heap_allocator()) };
	sprites[SPRITE_workbench] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/workbench.png"), get_heap_allocator()) };

	// @ship debug this off
	{
		for (SpriteID i = 0; i < SPRITE_MAX; i++) {
			Sprite* sprite = &sprites[i];
			assert(sprite->image, "Sprite was not setup properly");
		}
	}

	font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());

	// :building resource setup
	{
		// buildings[0] = 
		buildings[BUILDING_furnace] = (BuildingData){ .to_build=ARCH_furnace, .icon=SPRITE_furnace };
		buildings[BUILDING_workbench] = (BuildingData){ .to_build=ARCH_workbench, .icon=SPRITE_workbench };
	}

	// :init

	// test stuff
	// TODO - @ship debug this off
	{
		world->inventory_items[ARCH_item_pine_wood].amount = 5;
		// world->inventory_items[ARCH_item_rock].amount = 5;

		Entity* en = entity_create();
		setup_furnace(en);
	}

	Entity* player_en = entity_create();
	setup_player(player_en);

	for (int i = 0; i < 10; i++) {
		Entity* en = entity_create();
		setup_rock(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		// en->pos.y -= tile_width * 0.5;
	}
	for (int i = 0; i < 10; i++) {
		Entity* en = entity_create();
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		// en->pos.y -= tile_width * 0.5;
	}

	float64 seconds_counter = 0.0;
	s32 frame_count = 0;

	float zoom = 5.3;
	Vector2 camera_pos = v2(0, 0);

	float64 last_time = os_get_current_time_in_seconds();
	while (!window.should_close) {
		reset_temporary_storage();
		world_frame = (WorldFrame){0};
		float64 now = os_get_current_time_in_seconds();
		delta_t = now - last_time;
		last_time = now;
		os_update();

		world_frame.world_proj = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		// :camera
		{
			Vector2 target_pos = player_en->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f);

			world_frame.world_view = m4_make_scale(v3(1.0, 1.0, 1.0));
			world_frame.world_view = m4_mul(world_frame.world_view, m4_make_translation(v3(camera_pos.x, camera_pos.y, 0)));
			world_frame.world_view = m4_mul(world_frame.world_view, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}
		set_world_space();

		Vector2 mouse_pos_world = get_mouse_pos_in_world_space();
		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y);

		do_ui_stuff();

		{
			// log("%f, %f", mouse_pos_world.x, mouse_pos_world.y);
			// draw_text(font, sprint(temp, STR("%f %f"), mouse_pos_world.x, mouse_pos_world.y), font_height, mouse_pos_world, v2(0.1, 0.1), COLOR_RED);

			float smallest_dist = INFINITY;
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid && en->destroyable_world_item) {
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
			int player_tile_x = world_pos_to_tile_pos(player_en->pos.x);
			int player_tile_y = world_pos_to_tile_pos(player_en->pos.y);
			int tile_radius_x = 40;
			int tile_radius_y = 30;
			for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; x++) {
				for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; y++) {
					if ((x + (y % 2 == 0) ) % 2 == 0) {
						Vector4 col = v4(0.1, 0.1, 0.1, 0.1);
						float x_pos = x * tile_width;
						float y_pos = y * tile_width;
						draw_rect(v2(x_pos + tile_width * -0.5, y_pos + tile_width * -0.5), v2(tile_width, tile_width), col);
					}
				}
			}

			// draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
		}
		
		// :update entities
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {
				// pick up item
				if (en->is_item) {
					// TODO - epic physics pickup like arcana
					if (fabsf(v2_dist(en->pos, player_en->pos)) < player_pickup_radius) {
						world->inventory_items[en->arch].amount += 1;
						entity_destroy(en);
					}
				}
			}
		}

		// :click destroy
		{
			Entity* selected_en = world_frame.selected_entity;

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if (selected_en) {
					selected_en->health -= 1;
					if (selected_en->health <= 0) {

						switch (selected_en->arch) {
							case ARCH_tree: {
								// spawn thing
								{
									Entity* en = entity_create();
									setup_item_pine_wood(en);
									en->pos = selected_en->pos;
								}
							} break;

							case ARCH_rock: {
								Entity* en = entity_create();
								setup_item_rock(en);
								en->pos = selected_en->pos;
							} break;

							default: { } break;
						}

						entity_destroy(selected_en);
					}
				}
			}
		}

		// :render entities
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {

				switch (en->arch) {

					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						Matrix4 xform = m4_scalar(1.0);
						if (en->is_item) {
							xform         = m4_translate(xform, v3(0, 2.0 * sin_breathe(os_get_current_time_in_seconds(), 5.0), 0));
						}
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
			}
		}

		// @ship debug don't do this lol
		if (is_key_just_pressed(KEY_ESCAPE)) {
			window.should_close = true;
		}

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

		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, 100.0 * delta_t));

		gfx_update();
		seconds_counter += delta_t;
		frame_count += 1;
		if (seconds_counter > 1.0) {
			// log("fps: %i", frame_count);
			seconds_counter = 0.0;
			frame_count = 0;
		}
	}

	return 0;
}