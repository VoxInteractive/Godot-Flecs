shader_type canvas_item;
render_mode unshaded;

void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
	// Called for every pixel the material is visible on.
	vec4 vcol = COLOR; // instance/vertex color
	vec4 texc = texture(TEXTURE, UV); // your assigned texture
	COLOR = texc * vcol; // modulate by instance color
}

//void light() {
//	// Called for every pixel for every light affecting the CanvasItem.
//	// Uncomment to replace the default light processing function with this one.
//}
