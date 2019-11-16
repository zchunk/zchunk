node {
    checkout scm
    stage('Build') {
        parallel fedora: {
            
            sh "./autotest/fedora-latest/build.sh"
        },
        centos: {
            sh "./autotest/centos-7/build.sh"
        },
        opensuse: {
            sh "./autotest/opensuse-leap/build.sh"
        },
        debian: {
            sh "./autotest/debian-latest/build.sh"
        },
        ubuntu: {
            sh "./autotest/ubuntu-rolling/build.sh"
        }
    }
    stage('Test') {
        parallel fedora: {
            
            sh "./autotest/fedora-latest/test.sh"
        },
        centos: {
            sh "./autotest/centos-7/test.sh"
        },
        opensuse: {
            sh "./autotest/opensuse-leap/test.sh"
        },
        debian: {
            sh "./autotest/debian-latest/test.sh"
        },
        ubuntu: {
            sh "./autotest/ubuntu-rolling/test.sh"
        }
    }
}
