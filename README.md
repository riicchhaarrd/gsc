# GSC

Embeddable game scripting language implementation in C.

# Building
```sh
# libgsc
mkdir build
cd build
cmake ..
make

# gsc
cd ../examples
./build.sh
```

# Example
```c
/*
	Usually these events or callbacks with function pointers get called from C, but this serves as an example.
*/

game = {}; // optionally use a global variable set from within C with the API

my_coroutine1(dmg)
{
	// Coroutine instance will stop when this event happens.
	game endon("game_ended");
	
	// Coroutine will block until this event happens.
	game waittill("game_started");
	
	for(;;)
	{
		if(self.health <= 0) // Entity is dead
		{
			break;
		}
		self.health -= dmg;
		println("Health: " + self.health);
		wait 0.5; // Wait half a second.
	}
	// [[ self.callbacks.dead ]](); // Alternative form of calling function pointer
	self.callbacks.dead();
	println("Entity " + self.name + " is dead.");
}

callback()
{
	println("callback()");
}

main()
{
/*
	// You could also create objects internally with the API

	// Initialization
	static int f_sethealth(gsc_Context *ctx)
	{
		int obj = gsc_get_object(ctx, -1);
		YourInternalClass *e = reinterpret_cast<YourInternalClass*>(gsc_object_get_userdata(ctx, obj));
		e->health = gsc_get_int(ctx, 0);
		return 0;
	}

	int proxy = gsc_add_tagged_object(ctx, "Entity"); // Descriptive name which will show in debug contexts
	int methods = gsc_add_object(ctx);
	gsc_add_function(ctx, f_sethealth);
	gsc_object_set_field(ctx, methods, "sethealth");
	gsc_object_set_field(ctx, proxy, "__call"); // For field getter/setter use __set and __get
	gsc_set_global(ctx, "#entity");

	// Object creation
	int ent = gsc_add_object(ctx);
	int proxy = gsc_get_global(ctx, "#entity");
	gsc_object_set_proxy(ctx, ent, proxy);
	gsc_pop(ctx, 1);
*/
	e = {};
	e.position = (1, 2, 3); // vec3 variable type
	e.position += (0,0, 10);
	e.position *= 5;
	e.name = "Test Entity";
	e.health = 100;
	e.callbacks = {};
	e.callbacks.dead = ::callback; // Function pointer to callback function
	
	dmg = 3;
	
	e thread my_coroutine1(dmg);
	
	wait 2; // Wait 2 seconds
	println("[starting game]");
	game notify("game_started"); // You can notify this through C aswell.
	
	wait 5;
	println("[ending game]");
	game notify("game_ended");
}
```
