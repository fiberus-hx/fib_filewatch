package;

class TestMain {
	static function main() {
		var framework = new TestFramework();

		trace("--- TestFilewatch ---");
		var filewatchTests = new TestFilewatch();
		filewatchTests.setFramework(framework);
		filewatchTests.runTests();

		trace("");
		framework.printSummary();
	}
}
