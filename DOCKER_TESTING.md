Dockerfiles are available to test the latest build on different platforms:

 * fedora:latest - autotest/fedora-latest
 * centos:latest - autotest/centos-latest
 * ubuntu:rolling - autotest/ubuntu-rolling
 * opensuse/leap - autotest/opensuse-leap
 * debian:latest - autotest/debian-latest

To test, in the project root directory, run:<br>
```docker-compose --file autotest/<platform directory>/docker-compose.yml build```