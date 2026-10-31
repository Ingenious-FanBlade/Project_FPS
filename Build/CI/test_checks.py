"""Verify that common broken changes are rejected by the PR validator."""

from pathlib import Path
import tempfile
import unittest

from check_repository import validate_file


class RepositoryChecks(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)

    def check_text(self, name, content):
        path = self.root / name
        path.write_text(content, encoding="utf-8")
        return validate_file(path, self.root)

    def test_invalid_descriptor_and_yaml_fail(self):
        for name, text in [("Game.uproject", '{"Modules": [}'), ("job.yml", "jobs: [")]:
            with self.subTest(name=name):
                self.assertTrue(self.check_text(name, text))

    def test_missing_and_external_local_paths_fail(self):
        for link in ["missing.md", "../outside.md"]:
            with self.subTest(link=link):
                self.assertTrue(self.check_text("doc.md", f"[link]({link})"))

    def test_valid_links_and_code_examples_pass(self):
        (self.root / "image.png").write_bytes(b"reference")
        text = "![image](image.png)\n[web](https://example.com)\n```text\n[example](missing.md)\n```\n"
        self.assertEqual([], self.check_text("doc.md", text))

    def test_merge_conflicts_and_unclosed_fences_fail(self):
        self.assertTrue(self.check_text("code.cpp", "<<<<<<< HEAD\nint x;\n=======\nint y;\n>>>>>>> branch\n"))
        self.assertTrue(self.check_text("doc.md", "```text\nunfinished\n"))


if __name__ == "__main__":
    unittest.main()
