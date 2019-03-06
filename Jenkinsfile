pipeline {
  agent any
  stages {
    stage('Build') {
      steps {
        echo 'Building...'
        sh '''ls
export PATH=/usr/local/bin:$PATH
mkdir build
cd build
cmake .. -Dsqlite3=yes -Dcoverage=yes -Ddeveloper=yes
make -j4
make test
ctest -VV
'''
        sh 'ctest -V -R system'
      }
    }
    stage('Test') {
      steps {
        echo 'Testing ...'
      }
    }
    stage('Deploy') {
      steps {
        echo 'deploying'
      }
    }
  }
}