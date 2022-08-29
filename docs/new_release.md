# new releases

For developers. To create a new release you need to:

1. Pick a version.
2. Write a short changelog in markdown.
3. Add the changelog to RELEASES.md.
4. Update CMakeLists.txt with a new version.
5. Create a PR with all your changes, and merge it.
6. Pull your changes and build a new release. Create a .tar.gz package.
7. Draft a new release. Paste your changelog again, and attach the .tar.gz source.
8. Tag the dockerhub image with the new version.
9. Consider creating a PR for mquery with the new version.

