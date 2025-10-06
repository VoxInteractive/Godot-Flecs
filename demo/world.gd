extends FlecsWorld

func _process(delta: float) -> void:
    var age = get_age()
    print("World age: %s" % age)
