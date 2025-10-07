extends FlecsWorld

var _mm_initialized := false
var _mm_stride := 12
var cell_size := 8.0
var _frame_counter := 0
var _deferred_init := false
var _mm_buf: PackedFloat32Array = PackedFloat32Array()

func _ready() -> void:
	# Ensure ECS systems are registered and GoL grid is initialized from C++.
	# Call into the C++ method we bound as initialize_gol() when available.
	if has_method("initialize_gol"):
		call("initialize_gol")
	# Initialize the Multimesh once. We use get_alive_map() to determine the
	# required instance count, then allocate server-side storage and detect the
	# instance stride so subsequent updates can reuse the layout.
	var canvas := get_node_or_null("../Canvas")
	if canvas == null:
		canvas = get_node_or_null("Canvas")
	if canvas == null:
		push_error("Canvas node not found for multimesh initialization")
		return

	var mm = canvas.multimesh
	if mm == null:
		push_error("Canvas.multimesh is null")
		return

	var rows := get_alive_map()
	var h = rows.size()
	if h == 0:
		# Defer initialization until flecs C++ side has seeded the grid
		_deferred_init = true
		print("_ready: deferring multimesh initialization until alive map is available")
		# enable processing to retry initialization later
		set_process(true)
		set_physics_process(true)
		return
	var w = rows[0].size()
	var total = w * h

	# perform standard initialization now that map is present
	_init_multimesh(mm, total)

	# Ensure _process is called every frame (some nodes/scripts need explicit enabling)
	set_process(true)
	print("process enabled")

	# Ensure physics processing is enabled so FlecsWorld::_physics_process() runs.
	# That C++ method calls world.progress(), advancing the simulation.
	set_physics_process(true)
	print("physics process enabled")


func _init_multimesh(mm: MultiMesh, total: int) -> void:
	mm.instance_count = 0
	mm.set_transform_format(MultiMesh.TRANSFORM_2D)
	mm.set_use_colors(true)
	mm.instance_count = total
	RenderingServer.multimesh_allocate_data(mm.get_rid(), total, RenderingServer.MULTIMESH_TRANSFORM_2D, true, false, false)

	# Precompute and set transforms once using the API to avoid guessing buffer layout
	# Center the grid around origin for symmetry
	var rows := get_alive_map()
	var h := rows.size()
	var w: int = 0
	if h > 0:
		w = rows[0].size()
	for y in range(h):
		for x in range(w):
			var idx = y * w + x
			var tx = (float(x) - float(w) * 0.5) * cell_size
			var ty = (float(y) - float(h) * 0.5) * cell_size
			var xf := Transform2D()
			xf.x = Vector2(cell_size, 0.0)
			xf.y = Vector2(0.0, cell_size)
			xf.origin = Vector2(tx, ty)
			mm.set_instance_transform_2d(idx, xf)

	# Detect stride and capture the buffer with transforms baked by engine
	_mm_buf = RenderingServer.multimesh_get_buffer(mm.get_rid())
	if total > 0 and _mm_buf.size() > 0:
		_mm_stride = int(float(_mm_buf.size()) / float(total))
	else:
		_mm_stride = 12

	_mm_initialized = true

	# Optional: print small diagnostic once
	print("Multimesh initialized: instances=", total, " stride=", _mm_stride)

	# Ensure the MultiMeshInstance2D uses a 2D material that respects vertex colors.
	var canvas := get_node_or_null("../Canvas")
	if canvas == null:
		canvas = get_node_or_null("Canvas")
	if canvas and canvas.material == null:
		var shader := Shader.new()
		shader.code = """
			shader_type canvas_item;
			render_mode unshaded;
			void fragment() {
				vec4 albedo = texture(TEXTURE, UV);
				COLOR = albedo * COLOR;
			}
		"""
		var mat := ShaderMaterial.new()
		mat.shader = shader
		canvas.material = mat


func _process(_delta: float) -> void:
	# Render Game of Life state to the MultiMeshInstance2D named "Canvas".
	# This uses the MultiMesh API to set per-instance colors based on get_alive_map().
	# keep a simple frame counter for throttled diagnostics
	_frame_counter += 1
	var canvas := get_node_or_null("../Canvas")
	if canvas == null:
		canvas = get_node_or_null("Canvas")
	if canvas == null:
		if _frame_counter % 30 == 0:
			print("_process: Canvas node not found")
		return

	var mm = canvas.multimesh
	if mm == null:
		if _frame_counter % 30 == 0:
			print("_process: canvas.multimesh is null")
		return

	# For MultiMeshInstance2D, a Texture is used (not a Mesh). If none is set,
	# create a tiny white texture so per-instance vertex colors are visible.
	if canvas.texture == null:
		var img := Image.create(1, 1, false, Image.FORMAT_RGBA8)
		img.fill(Color(1, 1, 1, 1))
		var tex := ImageTexture.create_from_image(img)
		canvas.texture = tex

	var rows := get_alive_map()
	var h = rows.size()
	if h == 0:
		if _frame_counter % 30 == 0:
			print("_process: alive map empty or get_alive_map() returned empty rows")
		return
	var w = rows[0].size()

	# Lightweight diagnostics: print alive count every 30 frames
	_frame_counter += 1
	if _frame_counter % 30 == 0:
		var alive_count = 0
		for ry in range(h):
			var r = rows[ry]
			for rx in range(r.size()):
				if r[rx] == 1:
					alive_count += 1
		print("alive_count=", alive_count)

	var total = w * h

	# Ensure multimesh was initialized
	if not _mm_initialized:
		# If initialization hasn't happened yet, skip this frame (throttled message)
		if _frame_counter % 30 == 0:
			print("_process: multimesh not initialized yet (_mm_initialized=false)")
		return

	# Update only instance colors in our cached buffer and upload once
	var stride = _mm_stride
	if _mm_buf.size() != total * stride:
		_mm_buf.resize(total * stride)
	for y in range(h):
		var row = rows[y]
		for x in range(w):
			var idx = y * w + x
			var base = idx * stride
			var alive = (x < row.size()) and row[x] == 1
			var cr = 1.0 if alive else 0.2
			var cg = 1.0 if alive else 0.2
			var cb = 1.0 if alive else 0.2
			var ca = 1.0
			var color_start = 6 if stride == 10 else (8 if stride == 12 else max(6, stride - 4))
			_mm_buf[base + color_start + 0] = cr
			_mm_buf[base + color_start + 1] = cg
			_mm_buf[base + color_start + 2] = cb
			_mm_buf[base + color_start + 3] = ca

	RenderingServer.multimesh_set_buffer(mm.get_rid(), _mm_buf)

	# Minimal per-frame operation: bulk upload buffer only
