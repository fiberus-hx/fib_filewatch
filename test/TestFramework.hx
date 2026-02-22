package;

/**
 * Simple test framework for Fiberus
 * A lightweight alternative to utest that works with fiberus's current capabilities
 */
class TestFramework {
	public var passed:Int = 0;
	public var failed:Int = 0;
	public var currentTest:String = "";

	public function new() {}

	public function assertEqualsInt(expected:Int, actual:Int, ?message:String, ?pos:haxe.PosInfos):Bool {
		if (expected == actual) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : 'Expected $expected but got $actual';
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function assertEqualsString(expected:String, actual:String, ?message:String, ?pos:haxe.PosInfos):Bool {
		if (expected == actual) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : 'Expected "$expected" but got "$actual"';
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function assertEqualsDynamic(expected:Dynamic, actual:Dynamic, ?message:String, ?pos:haxe.PosInfos):Bool {
		if (expected == actual) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : 'Expected $expected but got $actual';
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function assertTrue(value:Bool, ?message:String, ?pos:haxe.PosInfos):Bool {
		if (value) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : "Expected true but got false";
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function assertFalse(value:Bool, ?message:String, ?pos:haxe.PosInfos):Bool {
		if (!value) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : "Expected false but got true";
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function assertNotNullDynamic(value:Dynamic, ?message:String, ?pos:haxe.PosInfos):Bool {
		if (value != null) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : "Expected non-null value";
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function assertFloatEquals(expected:Float, actual:Float, ?message:String, ?pos:haxe.PosInfos):Bool {
		var eps = 0.0001;
		var diff = expected - actual;
		if (diff < 0) diff = -diff;
		if (diff <= eps) {
			passed++;
			return true;
		} else {
			failed++;
			var msg = message != null ? message : 'Expected $expected but got $actual (diff=$diff)';
			trace('[FAIL] ${currentTest}: $msg at ${pos.fileName}:${pos.lineNumber}');
			return false;
		}
	}

	public function startTest(name:String) {
		currentTest = name;
		trace('  Running: $name');
	}

	public function printSummary() {
		trace("");
		trace("=== Test Summary ===");
		trace('Passed: $passed');
		trace('Failed: $failed');
		trace('Total:  ${passed + failed}');
		if (failed == 0) {
			trace("ALL TESTS PASSED!");
		} else {
			trace("SOME TESTS FAILED!");
		}
	}
}
