pipeline {
  agent any
  stages {
    stage('Build') {
      steps {
        echo 'Building...'
        sh '''export PATH=/usr/local/bin:$PATH
mkdir build
cd build
cmake .. -Dsqlite3=yes -Dcoverage=yes -Ddeveloper=yes
make -j4
'''
        sh '''export PATH=/usr/local/bin:$PATH
ctest -R gtest'''
        sh '''export PATH=/usr/local/bin:$PATH
ctest -R system'''
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