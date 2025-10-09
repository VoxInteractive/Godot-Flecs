extends FlecsWorld

# We now render Game of Life as a single texture on one quad for best performance.
# This avoids creating per-cell instances/colors and minimizes CPU <-> GPU traffic.

var _initialized := false
var _frame_counter := 0
var cell_size := 8.0

func _ready() -> void:
	# Ensure ECS systems are registered and GoL grid is initialized from C++.
	if has_method("initialize_gol"):
		call("initialize_gol")

	var canvas := get_node_or_null("../Canvas")
	if canvas == null:
		canvas = get_node_or_null("Canvas")
	if canvas == null:
		push_error("Canvas node not found")
		return

	# Initialize the texture once; size comes from C++ get_gol_texture().
	var tex := get_gol_texture()
	if tex == null:
		# If grid not ready yet, enable processing to complete init next frame.
		set_process(true)
		set_physics_process(true)
		return

	# Assign texture and configure sprite properties for crisp pixel rendering.
	canvas.texture = tex
	# Use nearest filtering so cells stay crisp when scaled.
	canvas.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	canvas.texture_repeat = CanvasItem.TEXTURE_REPEAT_DISABLED

	# Center the sprite and scale so 1 texel -> cell_size units.
	canvas.centered = true
	canvas.position = Vector2.ZERO
	canvas.scale = Vector2(cell_size, cell_size)

	# Minimal unshaded shader that displays L8 texture as white for alive (255) and black for dead (0).
	if canvas.material == null:
		var shader := Shader.new()
		shader.code = """
			shader_type canvas_item;
			render_mode unshaded;
			void fragment() {
				float v = texture(TEXTURE, UV).r;
				COLOR = vec4(vec3(v), 1.0);
			}
		"""
		var mat := ShaderMaterial.new()
		mat.shader = shader
		canvas.material = mat

	_initialized = true

	# Ensure per-frame updates and physics step (C++ advances ECS in _physics_process).
	set_process(true)
	set_physics_process(true)


func _init_multimesh(_mm: MultiMesh, _total: int) -> void:
	# Legacy no-op kept for compatibility; no longer used with texture-based rendering.
	pass


func _process(_delta: float) -> void:
	_frame_counter += 1

	var canvas := get_node_or_null("../Canvas")
	if canvas == null:
		canvas = get_node_or_null("Canvas")
	if canvas == null:
		if _frame_counter % 60 == 0:
			print("_process: Canvas node not found")
		return

	var tex := get_gol_texture()
	if tex == null:
		if _frame_counter % 60 == 0:
			print("_process: get_gol_texture() returned null")
		return

	if not _initialized:
		# Complete deferred init when texture becomes available.
		canvas.texture = tex
		canvas.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		canvas.texture_repeat = CanvasItem.TEXTURE_REPEAT_DISABLED
		canvas.centered = true
		canvas.position = Vector2.ZERO
		canvas.scale = Vector2(cell_size, cell_size)
		_initialized = true
		return

	# Per-frame: update the texture only if the Ref changed. C++ updates pixels in-place.
	if canvas.texture != tex:
		canvas.texture = tex
