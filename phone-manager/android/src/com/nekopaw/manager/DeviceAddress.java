package com.nekopaw.manager;
import java.net.URI;
final class DeviceAddress {
 static final String DEFAULT="http://192.168.1.100:81";
 static final String[] PATHS={"/pet","/","/weather","/calendar","/codex","/delivery","/memo"};
 static String normalize(String input){
  try{
   String value=input.trim();if(!value.contains("://"))value="http://"+value;
   URI u=new URI(value);String host=u.getHost();
   if(!"http".equalsIgnoreCase(u.getScheme())||host==null||u.getUserInfo()!=null||u.getQuery()!=null||u.getFragment()!=null)throw new Exception();
   boolean known=u.getPath().isEmpty();for(String p:PATHS)if(p.equals(u.getPath()))known=true;if(!known)throw new Exception();
   String[] parts=host.split("\\.");if(parts.length!=4)throw new Exception();int[] n=new int[4];
   for(int i=0;i<4;i++){if(!parts[i].matches("[0-9]{1,3}")||parts[i].length()>1&&parts[i].startsWith("0"))throw new Exception();n[i]=Integer.parseInt(parts[i]);if(n[i]>255)throw new Exception();}
   if(!(n[0]==10||n[0]==192&&n[1]==168||n[0]==172&&n[1]>=16&&n[1]<=31))throw new Exception();
   int port=u.getPort()<0?81:u.getPort();if(port<1||port>65535)throw new Exception();
   return "http://"+host+":"+port;
  }catch(Exception e){throw new IllegalArgumentException("请输入ESP32的局域网地址，例如 192.168.1.100:81");}
 }
 static boolean sameDevice(String base,String url){
  try{URI a=new URI(base),b=new URI(url);int port=b.getPort()<0?80:b.getPort();return "http".equalsIgnoreCase(b.getScheme())&&a.getHost().equals(b.getHost())&&a.getPort()==port&&b.getUserInfo()==null;}catch(Exception e){return false;}
 }
}
