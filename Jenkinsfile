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
      }
    }
    stage('Test') {
      steps {
        echo 'Testing ...'
        sh '''export PATH=/usr/local/bin:$PATH
cd build
ctest -R gtest'''
        sh '''export PATH=/usr/local/bin:$PATH
cd  build
ctest -R system'''
      }
    }
    stage('Deploy') {
      steps {
        echo 'deploying'
        deleteDir()
      }
    }
  }
}