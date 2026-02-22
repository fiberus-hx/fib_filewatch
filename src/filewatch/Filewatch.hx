package filewatch;

/**
 * Filewatch — Haxe/fiberus @:native binding for recursive file watching.
 *
 * Uses Linux inotify under the hood. Non-blocking: call update() periodically
 * to poll for events. The callback fires synchronously during update().
 *
 * Usage:
 *   Filewatch.init((type, rootPath, relPath) -> {
 *       trace('Event: $type $rootPath/$relPath');
 *   });
 *   Filewatch.addWatch("/path/to/watch");
 *   // In your main loop:
 *   Filewatch.update();
 *   // When done:
 *   Filewatch.shutdown();
 */
@:include("fib_filewatch.h")
@:sourceFile("../../lib/fib_filewatch.c")
extern class Filewatch {
	/**
	 * Initialize the file watcher with a callback.
	 * The callback receives (eventType:Int, rootPath:String, relPath:String).
	 * Must be called once before any other Filewatch function.
	 * Returns true on success.
	 */
	@:native("fib_filewatch_init") @:gcBlocking
	static function init(callback:(Int, String, String) -> Void):Bool;

	/**
	 * Add a directory to watch recursively.
	 * The path is resolved to an absolute path internally.
	 */
	@:native("fib_filewatch_add_watch") @:gcBlocking
	static function addWatch(path:String):Void;

	/**
	 * Remove a previously watched directory.
	 * Returns true if the path was found and removed.
	 */
	@:native("fib_filewatch_remove_watch") @:gcBlocking
	static function removeWatch(path:String):Bool;

	/**
	 * Poll for file system events (non-blocking).
	 * Fires the callback for each pending event.
	 * Call this periodically (e.g., each frame or tick).
	 */
	@:native("fib_filewatch_update") @:gcBlocking
	static function update():Void;

	/**
	 * Shut down the file watcher and free all resources.
	 * The callback will not be fired after this call.
	 */
	@:native("fib_filewatch_shutdown") @:gcBlocking
	static function shutdown():Void;
}
