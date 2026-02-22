package;

import filewatch.Filewatch;

/**
 * Tests for the fib_filewatch native callback binding.
 *
 * These tests verify that:
 * 1. Haxe closures can be passed to C as function pointer callbacks
 * 2. The callback trampoline correctly marshals C args (int, string, string) to Haxe
 * 3. File create/modify/delete events are detected via inotify
 * 4. The callback fires synchronously during update()
 */
class TestFilewatch extends Test {
	public function runTests() {
		testInitShutdown();
		testFileCreate();
		testFileModify();
		testCallbackStringArgs();
		testMultipleUpdates();
	}

	/**
	 * Test that init + shutdown works without crashing.
	 */
	function testInitShutdown() {
		framework.startTest("testInitShutdown");
		var called = false;
		var result = Filewatch.init((type, root, rel) -> {
			called = true;
		});
		t(result); // init should return true
		Filewatch.shutdown();
		f(called); // callback should not have been called
	}

	/**
	 * Test that we can detect a file creation event.
	 * Creates a temp directory, watches it, creates a file,
	 * then polls for events.
	 */
	function testFileCreate() {
		framework.startTest("testFileCreate");
		// Create a temp directory
		var tmpDir = "/tmp/fib_filewatch_test_" + Std.string(Std.random(1000000));
		sys.FileSystem.createDirectory(tmpDir);

		var events:Array<{type:Int, root:String, rel:String}> = [];

		var inited = Filewatch.init((type, root, rel) -> {
			events.push({type: type, root: root, rel: rel});
		});
		t(inited);

		Filewatch.addWatch(tmpDir);

		// Small delay to let inotify settle
		Sys.sleep(0.05);

		// Create a file
		sys.io.File.saveContent(tmpDir + "/test.txt", "hello");

		// Poll for events (may need a small delay for inotify)
		Sys.sleep(0.05);
		Filewatch.update();

		// Should have received at least one CREATE event (type=3)
		t(events.length > 0);

		var hasCreate = false;
		for (evt in events) {
			if (evt.type == 3 && evt.rel == "test.txt") { // FIB_FW_CREATE = 3
				hasCreate = true;
			}
		}
		t(hasCreate);

		// Clean up
		Filewatch.shutdown();
		sys.FileSystem.deleteFile(tmpDir + "/test.txt");
		sys.FileSystem.deleteDirectory(tmpDir);
	}

	/**
	 * Test that we can detect a file modification event.
	 */
	function testFileModify() {
		framework.startTest("testFileModify");
		var tmpDir = "/tmp/fib_filewatch_test_mod_" + Std.string(Std.random(1000000));
		sys.FileSystem.createDirectory(tmpDir);

		// Create the file before watching
		sys.io.File.saveContent(tmpDir + "/existing.txt", "original");

		var events:Array<{type:Int, root:String, rel:String}> = [];

		Filewatch.init((type, root, rel) -> {
			events.push({type: type, root: root, rel: rel});
		});
		Filewatch.addWatch(tmpDir);

		Sys.sleep(0.05);

		// Modify the file
		sys.io.File.saveContent(tmpDir + "/existing.txt", "modified");

		Sys.sleep(0.05);
		Filewatch.update();

		// Should have received a MODIFY event (type=1)
		var hasModify = false;
		for (evt in events) {
			if (evt.type == 1 && evt.rel == "existing.txt") { // FIB_FW_MODIFY = 1
				hasModify = true;
			}
		}
		t(hasModify);

		// Clean up
		Filewatch.shutdown();
		sys.FileSystem.deleteFile(tmpDir + "/existing.txt");
		sys.FileSystem.deleteDirectory(tmpDir);
	}

	/**
	 * Test that the callback receives correct string arguments
	 * (root path and relative file path).
	 */
	function testCallbackStringArgs() {
		framework.startTest("testCallbackStringArgs");
		var tmpDir = "/tmp/fib_filewatch_test_str_" + Std.string(Std.random(1000000));
		sys.FileSystem.createDirectory(tmpDir);

		var receivedRoot:String = "";
		var receivedRel:String = "";

		Filewatch.init((type, root, rel) -> {
			if (type == 3) { // CREATE
				receivedRoot = root;
				receivedRel = rel;
			}
		});
		Filewatch.addWatch(tmpDir);

		Sys.sleep(0.05);

		sys.io.File.saveContent(tmpDir + "/callback_test.txt", "test");

		Sys.sleep(0.05);
		Filewatch.update();

		// Root should be the absolute path of tmpDir
		t(receivedRoot.length > 0);
		eqStr(receivedRel, "callback_test.txt");

		// Clean up
		Filewatch.shutdown();
		sys.FileSystem.deleteFile(tmpDir + "/callback_test.txt");
		sys.FileSystem.deleteDirectory(tmpDir);
	}

	/**
	 * Test that multiple update() calls work correctly
	 * and events don't repeat.
	 */
	function testMultipleUpdates() {
		framework.startTest("testMultipleUpdates");
		var tmpDir = "/tmp/fib_filewatch_test_multi_" + Std.string(Std.random(1000000));
		sys.FileSystem.createDirectory(tmpDir);

		var eventCount = 0;

		Filewatch.init((type, root, rel) -> {
			eventCount++;
		});
		Filewatch.addWatch(tmpDir);

		Sys.sleep(0.05);

		// First update with no changes
		Filewatch.update();
		eq(eventCount, 0);

		// Create a file
		sys.io.File.saveContent(tmpDir + "/file1.txt", "data");
		Sys.sleep(0.05);
		Filewatch.update();

		var afterFirst = eventCount;
		t(afterFirst > 0);

		// Second update with no new changes
		Filewatch.update();
		eq(eventCount, afterFirst); // Should not increase

		// Clean up
		Filewatch.shutdown();
		sys.FileSystem.deleteFile(tmpDir + "/file1.txt");
		sys.FileSystem.deleteDirectory(tmpDir);
	}
}
