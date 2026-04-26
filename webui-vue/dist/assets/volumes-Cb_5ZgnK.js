function r(n){return!n||typeof n!="object"?"":String(n.encryptionkey??n.EncryptionKey??n.encrkey??n.EncrKey??"").trim()}function t(n){return r(n).length>0}export{t as v};
