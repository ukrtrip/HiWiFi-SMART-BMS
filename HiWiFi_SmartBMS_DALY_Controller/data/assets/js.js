 var getJSON = function(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'json';
    xhr.onload = function() {
      var status = xhr.status;
      if (status === 200) { callback(null, xhr.response); } else { callback(status, xhr.response); }
    };
    xhr.send();
 };
function onLoad(e) {
 setTimeout(getVal,200);
 setTimeout(heart,500);
 contButInit();
}
function onLoadControl(e) {
 setTimeout(getValCon,200);
 contButInit();
 contEnBut();
}
function onLoadSetup(e) {
 setTimeout(scanWiFi,1500);
}
function scanWiFi() {
 var tab = document.getElementById('slist');
 getJSON("/scan", function(err, j) {
 if (err === null) {
   j.sort(GSO("rssi"));   
   tab.innerHTML = "";
   for(var k in j) {
    tab.innerHTML += "<li><div class='tl'>"+j[k]['ssid']+"</div><div>"+j[k]['rssi']+"</div><div>"+j[k]['channel']+"</div></li>";
   }
   var tls	= document.getElementsByClassName('tl');
   for(let tl of tls) {
	tl.addEventListener("click",function(e){document.getElementById('ssid').value=e.srcElement.outerText;});
   }
  }
 });
 setTimeout(scanWiFi,15000);
}
function GSO(prop) {
 return function(a, b){if (a[prop] > b[prop]){return 1;} else if (a[prop] < b[prop]){return -1;} return 0;}    
}    
function s2t(sec) {
 var hh = Math.floor(sec/3600);
 var mm = Math.floor((sec-(hh*3600))/60);
 var ss = sec-(hh*3600)-(mm*60);
 return hh.toString().padStart(2,'0')+':'+mm.toString().padStart(2,'0')+':'+ss.toString().padStart(2,'0'); 
}
function getVal(){
 getJSON('/json',function(err, j) {
  if (err === null) {
   for(var k in j) {
    var el = document.getElementById(k);
    if (el !== null) {
     if(k == "uptime"){j[k] = s2t(j[k]);
	 }else if(k=="cFet"||k=="dFet"||k=="balancer"){
	  if(j[k]==1){el.checked=true;document.getElementById(k+'T').innerHTML="ON";}else{el.checked=false;document.getElementById(k+'T').innerHTML="OFF";}
     }else if(k=="maxCV"||k=="minCV"||k=="difCV"){j[k] = (j[k]/1000).toFixed(3);
	 }else if(k=="cellsV"){for(var n in j[k]){j[k][n]=' '+(j[k][n]/1000).toFixed(3);}
	 }else if(k=="mqtt"){if(j[k]==1)j[k]="<green>connected</green>";else j[k]="offline";}
	 el.innerHTML = j[k];
    }
   }
  }
 });
 setTimeout(getVal,5050);
}
function contButInit() {
 var bu=document.getElementsByClassName('but');
 for(let b of bu) {
  b.addEventListener("click",function(e){
   var xhr = new XMLHttpRequest();
   var d = e.srcElement.id;
   var v = e.srcElement.checked;
   xhr.open('POST','/bms', true);
   xhr.setRequestHeader('Content-type','application/x-www-form-urlencoded');
   xhr.send(d+"="+v);
   if(v==true)document.getElementById(d+'T').innerHTML="ON";else document.getElementById(d+'T').innerHTML="OFF";
  });
 }
}
function getValCon(){
 getJSON('/json',function(err,j) {
  if (err === null) {
   for(var k in j) {
	var e = document.getElementById(k);
    if (e !== null) { 
	 if(k=="cFet"||k=="dFet"){
	  if(j[k]==1){e.checked=true;document.getElementById(k+'T').innerHTML="ON";}
	  else{e.checked=false;document.getElementById(k+'T').innerHTML="OFF";}  
	 } else if(k=="mqtt"){
	  if(j[k]==1)e.style.display="block"; else e.style.display="none";
	 }
	}	
   }	
  }
 });
 setTimeout(getValCon,14000);
}
function contEnBut() {
 var mc = document.getElementById('mc');
 var ms = document.getElementById('ms');
 var tc = document.getElementById('tc');
 var ts = document.getElementById('ts');
 if(ms.value == 1)mc.checked = true; else mc.checked = false;
 if(ts.value == 1)tc.checked = true; else tc.checked = false;
 mc.addEventListener("click",function(e){if(e.srcElement.checked==true)ms.value=1; else ms.value=0;});
 tc.addEventListener("click",function(e){if(e.srcElement.checked==true)ts.value=1; else ts.value=0;});
}
function heart(){
 if(document.getElementById("bh").innerHTML == document.getElementById("bh2").innerHTML) {
  document.getElementById("heart").style.animation = "beat 4s infinite alternate";
 } else {
  document.getElementById("bh2").innerHTML = document.getElementById("bh").innerHTML;
  document.getElementById("heart").style.animation = "beat .45s infinite alternate";
 }
  setTimeout(heart,2500);
}
function setForm() {
  var p=document.forms["m"]["m1"].value;
  var i=document.forms["m"]["m5"].value;
  var pt=document.forms["m"]["t1"].value;
  var err=0;
  document.querySelectorAll(".errmes").forEach(a=>a.style.display="none");
  if(isNaN(p)||(p<1||p>65535)){err=1;document.getElementById('m1e').style.display="block";}
  if(isNaN(i)||(i<5||i>3600)){err=1;document.getElementById('m5e').style.display="block";}
  if(isNaN(pt)||(pt<1||pt>65535)){err=1;document.getElementById('t1e').style.display="block";}
  if(err==1)return false;
}
