node {
    stage('Prep') {
        git "https://github.com/zchunk/zchunk.git"
    }
    stage('Build') {
        parallel fedora: {
            
            sh "./autotest/fedora-latest/build.sh"
        },
        centos: {
            sh "./autotest/centos-latest/build.sh"
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
            sh "./autotest/centos-latest/test.sh"
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
