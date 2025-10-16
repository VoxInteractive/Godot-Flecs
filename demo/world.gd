extends FlecsWorld

@onready var canvas: Sprite2D = $"../Canvas"

var tex: ImageTexture

func _ready() -> void:
	call("initialize_game_of_life")
	
	# Ensure the canvas sprite starts at the top-left and fills the viewport.
	canvas.centered = false
	canvas.position = Vector2.ZERO
	var rf := get_resolution_factor()
	if rf > 0.0:
		canvas.scale = Vector2(1.0 / rf, 1.0 / rf)
	canvas.texture = tex

func _process(_delta: float) -> void:
	# would subviewport be faster?
	var rf := get_resolution_factor()
	if rf > 0.0: # Keep the sprite scaled so the texture always fills the viewport.
		canvas.scale = Vector2(1.0 / rf, 1.0 / rf)
	canvas.texture = get_game_of_life_texture()
