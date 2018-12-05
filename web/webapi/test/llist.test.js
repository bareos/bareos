'use strict'

const chai = require('chai')
const chaiHttp = require('chai-http')

chai.use(chaiHttp)

const app = require('../api/server')

describe('API', () => {
  describe('llist', () => {
    describe('clients', () => {
      it('should return a list of clients 1.', done => {
        chai.request(app.listen())
          .get('/api2/command/run')
          .end((err, res) => {
            done()
          })
      })
    })
    describe('clients', () => {
      it('should return a list of clients 2.', done => {
        chai.request(app.listen())
          .get('/api2/command/run')
          .end((err, res) => {
            done()
          })
      })
    })
  })
})