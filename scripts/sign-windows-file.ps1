# Optional Authenticode signing for both the installed launcher and its distributable wrapper.
# Configure a trusted code-signing PFX as GitHub secrets; no certificate is stored in the source tree.
param([Parameter(Mandatory=$true)][string]$FilePath)
$ErrorActionPreference = 'Stop'
if (-not $env:XYLARJAVA_SIGNING_CERT_BASE64) {
    Write-Host 'No code-signing certificate configured; this build is unsigned.'
    return
}
$bytes = [Convert]::FromBase64String($env:XYLARJAVA_SIGNING_CERT_BASE64)
$cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
    $bytes, $env:XYLARJAVA_SIGNING_PASSWORD,
    [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet)
try {
    if (-not $cert.HasPrivateKey) { throw 'Code-signing certificate has no private key.' }
    $signature = Set-AuthenticodeSignature -FilePath $FilePath -Certificate $cert -HashAlgorithm SHA256 -TimestampServer 'http://timestamp.digicert.com'
    if ($signature.Status -ne 'Valid') { throw "Authenticode signing failed: $($signature.StatusMessage)" }
} finally { $cert.Dispose(); [Array]::Clear($bytes, 0, $bytes.Length) }
