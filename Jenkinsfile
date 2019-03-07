pipeline {
  agent any
  stages {
    stage('Build') {
      steps {
        echo 'Building...'
        sh '''export PATH=/usr/local/bin:$PATH
cd obs
./configure_obs.sh
'''
      }
    }
    stage('Test') {
      steps {
        echo 'Testing ...'
        sh '''export PATH=/usr/local/bin:$PATH
cd build
ctest -R gtest -D Experimental'''
        sh '''export PATH=/usr/local/bin:$PATH
cd  build
ctest -R system:backup-bareos-test -D Experimental'''
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
