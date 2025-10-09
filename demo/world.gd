extends FlecsWorld

@onready var canvas: Sprite2D = $"../Canvas"

var tex: ImageTexture

func _ready() -> void:
	if has_method("initialize_gol"):
		call("initialize_gol")

	canvas.texture = tex

func _init_multimesh(_mm: MultiMesh, _total: int) -> void:
	# Legacy no-op kept for compatibility; no longer used with texture-based rendering.
	pass

func _process(_delta: float) -> void:
	# would subviewport be faster?
	canvas.texture = get_gol_texture()
