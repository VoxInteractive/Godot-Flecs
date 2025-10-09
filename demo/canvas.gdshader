shader_type canvas_item;
render_mode unshaded;
void fragment() {
	float v = texture(TEXTURE, UV).r;
	COLOR = vec4(vec3(v), 1.0);
}
