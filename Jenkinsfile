pipeline {
  agent {
    label 'osc'
  }
  stages {
    stage('Build in OBS') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd obs
env
export GIT_BRANCH
sh -x ./configure_obs.sh
'''
      }
    }
    stage('Test in Jenkins') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd obs
sh -x ./wait_for_completion.sh
'''
      }
    }
    stage('Configure  Jenkins') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd jenkins
env
export GIT_BRANCH
sh -x ./configure_jenkins.sh
'''
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
