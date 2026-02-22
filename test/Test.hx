package;

/**
 * Base test class for Fiberus tests
 * Uses a simple test framework compatible with fiberus's current capabilities
 */
class Test {
	public var framework:TestFramework;

	public function new() {
		framework = new TestFramework();
	}

	public function setFramework(f:TestFramework) {
		framework = f;
	}

	function eq(expected:Int, actual:Int, ?pos:haxe.PosInfos) {
		framework.assertEqualsInt(expected, actual, null, pos);
	}

	function eqStr(expected:String, actual:String, ?pos:haxe.PosInfos) {
		framework.assertEqualsString(expected, actual, null, pos);
	}

	function eqDyn(expected:Dynamic, actual:Dynamic, ?pos:haxe.PosInfos) {
		framework.assertEqualsDynamic(expected, actual, null, pos);
	}

	function feq(expected:Float, actual:Float, ?pos:haxe.PosInfos) {
		framework.assertFloatEquals(expected, actual, null, pos);
	}

	function t(value:Bool, ?pos:haxe.PosInfos) {
		framework.assertTrue(value, null, pos);
	}

	function f(value:Bool, ?pos:haxe.PosInfos) {
		framework.assertFalse(value, null, pos);
	}

	function notNull(value:Dynamic, ?pos:haxe.PosInfos) {
		framework.assertNotNullDynamic(value, null, pos);
	}

	function noAssert(?pos:haxe.PosInfos) {
		framework.assertTrue(true, null, pos);
	}
}
