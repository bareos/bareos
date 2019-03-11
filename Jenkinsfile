pipeline {
  agent {
    label 'osc'
  }
  stages {
    stage('Configure OBS') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd pipeline/obs
env
export GIT_BRANCH
sh -x ./configure_obs.sh
'''
      }
    }
    stage('Wait for OBS to complete') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd pipeline/obs
sh -x ./wait_for_completion.sh
'''
      }
    }
    stage('Configure Jenkins and run tests') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd pipeline/jenkins
env
export GIT_BRANCH
sh -x ./configure_jenkins.sh
'''
      }
    }
    stage('Deploy') {
      steps {
        echo 'deploying'
      }
    }
  }
}
