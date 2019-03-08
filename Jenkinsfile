pipeline {
  agent  master
  stages {
    stage('Configure OBS') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd obs
env
export GIT_BRANCH
sh -x ./configure_obs.sh
'''
      }
    }
    stage('Build') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd obs
sh -x ./wait_for_completion.sh
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
