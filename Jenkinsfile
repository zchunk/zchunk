node {
    checkout scm
    stage('Build') {
        sh "./autotest/fedora-latest/build.sh"
        sh "./autotest/centos-7/build.sh"
        sh "./autotest/opensuse-leap/build.sh"
        sh "./autotest/alpine-edge/build.sh"
        sh "./autotest/debian-latest/build.sh"
        sh "./autotest/ubuntu-rolling/build.sh"
    }
    stage('Test') {
        sh "./autotest/fedora-latest/test.sh"
        sh "./autotest/centos-7/test.sh"
        sh "./autotest/opensuse-leap/test.sh"
        sh "./autotest/alpine-edge/test.sh"
        sh "./autotest/debian-latest/test.sh"
        sh "./autotest/ubuntu-rolling/test.sh"
    }
}
