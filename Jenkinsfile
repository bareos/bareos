pipeline {
  options {
    disableConcurrentBuilds()
  }
  agent {
    label 'osc'
  }
  stages {
    stage('checkout CD scripts') {
      steps {
        checkout([$class: 'GitSCM', branches: [[name: '*/master']], doGenerateSubmoduleConfigurations: false, extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'CD']], submoduleCfg: [], userRemoteConfigs: [[credentialsId: '742690a3-0716-4bd8-8b0b-eec6576d5962', url: 'https://git.bareos.com/pstorz/CD']]])

      }
    }
    stage('calculate environment') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd CD
sh -x ./calculate_environment.sh
'''
      }
    }
    stage('Configure OBS') {
      steps {

        sh '''export PATH=/usr/local/bin:$PATH
cd CD/obs
env
export GIT_BRANCH
sh -x ./configure_obs.sh
'''
      }
    }
    stage('Wait for OBS to complete') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/obs
sh -x ./wait_for_completion.sh
'''
        recordIssues enabledForFailure: true, tool: gcc4()
      }
    }
    stage('Fetch packages from OBS as Artifacts') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/obs
sh -x ./get_packages_from_obs.sh
'''
archiveArtifacts artifacts: 'packages/**/*', excludes: 'packages/*/repocache/**'

      }
    }
    stage('Configure Jenkins and run tests on internal repo') {
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/jenkins
env
export GIT_BRANCH
sh -x ./configure_jenkins.sh
'''
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/jenkins
env
export GIT_BRANCH
sh -x ./wait_for_jenkins_to_complete.sh
'''
      }
    }
    stage('Deploy') {
      when {
        anyOf {
          branch 'master'
          branch 'bareos-??.?'
        }
      }
      steps {
        echo 'deploying'
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/publish
sh -x ./create_publish_config.sh
'''
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/publish
sh -x ./publish_repo.sh
'''
      }
    }
    stage('Configure Jenkins and run tests on published repo') {
      when {
        anyOf {
          branch 'master'
          branch 'bareos-??.?'
        }
      }
      steps {
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/jenkins
env
export GIT_BRANCH
sh -x ./configure_jenkins_for_published.sh
'''
        sh '''export PATH=/usr/local/bin:$PATH
cd CD/jenkins
env
export GIT_BRANCH
sh -x ./wait_for_jenkins_to_complete.sh
'''
      }
    }
  }
  /* post { */
  /*     always { */
  /*         recordIssues enabledForFailure: true, tool: gcc4() */
  /*     } */
  /* } */
}
