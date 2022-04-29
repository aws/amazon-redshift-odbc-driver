$charArray = [char[]]$args[0]

( Get-content paversion.h ) | 
ForEach-Object { $_ -replace "01.01.0000", ($charArray[0]+"."+$charArray[2]+"."+$charArray[4]+"."+$charArray[6]) } | 
Set-Content paversion.h

( Get-content paversion.h ) | 
ForEach-Object { $_ -replace "1,0,0,1", ($charArray[0]+","+$charArray[2]+","+$charArray[4]+","+$charArray[6]) } | 
Set-Content paversion.h

( Get-content paversion.h ) | 
ForEach-Object { $_ -replace "1, 0, 0, 1", ($charArray[0]+", "+$charArray[2]+", "+$charArray[4]+", "+$charArray[6]) } | 
Set-Content paversion.h