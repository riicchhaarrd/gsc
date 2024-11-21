/*
	Usage:
	./build.sh
	./gsc example

	Usually these events or callbacks with function pointers get called from C, but this serves as a example.
*/

game = {}; // optionally use a global set from within C

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
	
	e = {};
	e.name = "Test Entity";
	e.health = 100;
	e.callbacks = {};
	e.callbacks.dead = ::callback;
	
	dmg = 3;
	
	e thread my_coroutine1(dmg);
	
	wait 2; // Wait 2 seconds
	println("[starting game]");
	game notify("game_started"); // You can notify this through C aswell.
	
	wait 5;
	println("[ending game]");
	game notify("game_ended");
}
