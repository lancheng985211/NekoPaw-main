package com.nekopaw.manager;
public final class DeviceAddressTest {
 static void equal(String a,String b){if(!a.equals(b))throw new AssertionError(a+" != "+b);}
 public static void main(String[] args){
  equal(DeviceAddress.normalize("192.168.1.100"),DeviceAddress.DEFAULT);
  equal(DeviceAddress.normalize("  http://192.168.1.100:81/memo  "),DeviceAddress.DEFAULT);
  equal(DeviceAddress.normalize("192.168.4.1:81/"),"http://192.168.4.1:81");
  equal(DeviceAddress.normalize("172.16.0.2:80"),"http://172.16.0.2:80");
  String[] bad={"https://10.1.1.1","http://8.8.8.8","http://127.0.0.1","http://010.1.1.1","http://192.168.1.999","http://192.168.1.1:0","http://192.168.1.1:65536","http://user@10.1.1.1","http://10.1.1.1?x=1","http://10.1.1.1/#x","file:///sdcard/test","javascript:alert(1)","http://10.1.1.1/unknown","http://10.1.1.1.evil.test/","172.32.1.1"};
  for(String value:bad){boolean rejected=false;try{DeviceAddress.normalize(value);}catch(IllegalArgumentException e){rejected=true;}if(!rejected)throw new AssertionError(value);}
  if(!DeviceAddress.sameDevice(DeviceAddress.DEFAULT,DeviceAddress.DEFAULT+"/api/memo"))throw new AssertionError();
  if(DeviceAddress.sameDevice(DeviceAddress.DEFAULT,"http://192.168.1.100:80/memo"))throw new AssertionError();
  if(DeviceAddress.sameDevice(DeviceAddress.DEFAULT,"http://192.168.1.100.evil.test:81/"))throw new AssertionError();
  if(DeviceAddress.sameDevice(DeviceAddress.DEFAULT,"file:///test"))throw new AssertionError();
  System.out.println("PASS private IPv4 normalization, port 81 default, feature URLs, invalid inputs, exact WebView origin restriction");
 }
}
