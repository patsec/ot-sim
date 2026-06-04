import datetime
from pathlib import Path
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography import x509
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID


from client_helper.utils import CADoesNotExist, CertExistsError, PrivateKeyDeosntExist, TLSWrap

class CryptographyWrapper(TLSWrap):
    @staticmethod
    def tls_create_private_key(file_path: Path) -> bool:
        """
        Creates a private key in the path that is specified.  The path will be overwritten
        if it already exists.

        Args:
            file_path:

        Returns:

        """
        pk = ec.generate_private_key(ec.SECP224R1(), default_backend())
        result = pk.private_bytes(encoding=serialization.Encoding.PEM,
                         format=serialization.PrivateFormat.PKCS8,
                         encryption_algorithm=serialization.NoEncryption())
        file_path.parent.mkdir(parents=True, exist_ok=True)
        file_path.open("wb").write(result)
        return True


    @staticmethod
    def tls_create_ca_certificate(common_name: str, private_key_file: Path, ca_cert_file: Path):
        """
        Create a ca certificate from using common name private key and ca certificate file.

        Args:
            common_name:
            private_key_file:
            ca_cert_file:

        Returns:

        """
        if ca_cert_file.exists():
            raise CertExistsError(ca_cert_file)
        
        if not private_key_file.exists():
            CryptographyWrapper.tls_create_private_key(private_key_file)
        
        pk = serialization.load_pem_private_key(
            private_key_file.read_bytes(), None, default_backend())
        
        # Create CSR for the CA Cetificate
        # csr = x509.CertificateSigningRequestBuilder().subject_name(
        #     x509.Name([
        #         x509.NameAttribute(NameOID.COMMON_NAME, "CA")
        #         # Provide various details about who we are.
        #         # x509.NameAttribute(NameOID.COUNTRY_NAME, u"US"),
        #         # x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, u"California"),
        #         # x509.NameAttribute(NameOID.LOCALITY_NAME, u"San Francisco"),
        #         # x509.NameAttribute(NameOID.ORGANIZATION_NAME, u"My Company"),
        #         # x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        #     ])).add_extension(
        #         x509.SubjectAlternativeName([
        #         # Describe what sites we want this certificate for.
        #         # x509.DNSName(u"mysite.com"),
        #         # x509.DNSName(u"www.mysite.com"),
        #         x509.DNSName(common_name),
        #     ]),
        #         critical=False,
        #     # Sign the CSR with our private key.
        #     ).sign(pk, hashes.SHA256())
        
        ca_subject = x509.Name([
            x509.NameAttribute(NameOID.COMMON_NAME, common_name)
        ])
        
        # # Various details about who we are. For a self-signed certificate the
        # # subject and issuer are always the same.
        # subject = issuer = x509.Name([
        #             x509.NameAttribute(NameOID.COUNTRY_NAME, u"US"),
        #             x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, u"California"),
        #             x509.NameAttribute(NameOID.LOCALITY_NAME, u"San Francisco"),
        #             x509.NameAttribute(NameOID.ORGANIZATION_NAME, u"My Company"),
        #             x509.NameAttribute(NameOID.COMMON_NAME, u"mysite.com"),
        #         ])

        cert = x509.CertificateBuilder().subject_name(
                    ca_subject
                ).issuer_name(
                    ca_subject
                ).public_key(
                    pk.public_key()
                ).serial_number(
                    x509.random_serial_number()
                ).not_valid_before(
                    datetime.datetime.now(datetime.timezone.utc)
                ).not_valid_after(
                    # Our certificate will be valid for 10 days
                    datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=20*365)
                ).add_extension(
                    x509.BasicConstraints(ca=True, path_length=None), critical=True
                 # Sign our certificate with our private key
                ).sign(pk, hashes.SHA256())
        
        ca_cert_file.write_bytes(cert.public_bytes(serialization.Encoding.PEM))

    @staticmethod
    def tls_create_csr(common_name: str,  private_key_file: Path, server_csr_file: Path):
        """

        Args:
            common_name:
            private_key_file:
            server_csr_file:

        Returns:

        """
        csr = x509.CertificateSigningRequest().subject_name(x509.Name([
            x509.NameAttribute(NameOID.COMMON_NAME, common_name)
        ])).add_extention(
            x509.SubjectAlternativeName([
                x509.DNSName(common_name)
            ]),
            critical=True
        ).sign(key, hashes.SHA256())
        
    @staticmethod
    def tls_create_device_certificate(ipaddress: str,
                                      ca_key_file: Path,
                                      ca_cert_file: Path,
                                      private_key_file: Path,
                                      cert_file: Path):
        pass

    @staticmethod
    def tls_create_signed_certificate(common_name: str,
                                      ca_key_file: Path,
                                      ca_cert_file: Path,
                                      private_key_file: Path,
                                      cert_file: Path,
                                      as_server: bool = False):
        """

        Args:
            common_name:
            ca_key_file:
            ca_cert_file:
            private_key_file:
            cert_file:
            as_server:

        Returns:

        """
        
        if not ca_key_file.exists() or not ca_cert_file.exists():
            raise CADoesNotExist()
        
        if not private_key_file.exists():
            raise PrivateKeyDeosntExist(private_key_file)
        
        if cert_file.exists():
            raise CertExistsError(cert_file)
        
        pk = serialization.load_pem_private_key(
            private_key_file.read_bytes(), None, default_backend())
        
        signing_key = serialization.load_pem_private_key(
            ca_key_file.read_bytes(), None, default_backend())
        
        signing_cert = x509.load_pem_x509_certificate(
            ca_cert_file.read_bytes(), default_backend()
        )
        
        san = x509.SubjectAlternativeName([x509.DNSName(common_name)])
        
        builder = x509.CertificateBuilder().subject_name(
                    x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "")])
                ).issuer_name(
                    signing_cert.subject
                ).public_key(
                    pk.public_key()
                ).serial_number(
                    x509.random_serial_number()
                ).not_valid_before(
                    datetime.datetime.now(datetime.timezone.utc)
                ).not_valid_after(
                    # Our certificate will be valid for 10 days
                    datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=20*365)
                ).add_extension(
                    san, False
                 # Sign our certificate with our private key
                )
                
        # builder = builder.add_extension(
        #     x509.KeyUsage(digital_signature=True, key_encipherment=True,
        #                   content_commitment=False,
        #                   data_encipherment=False, key_agreement=False,
        #                   key_cert_sign=False,
        #                   crl_sign=False,
        #                   encipher_only=False, decipher_only=False
        #                   ),
        #         critical=True)
        
        if as_server:
            builder = builder.add_extension(
                x509.ExtendedKeyUsage((ExtendedKeyUsageOID.SERVER_AUTH,)),
                critical=False
            )
        cert = builder.sign(signing_key, hashes.SHA256())
        cert_file.write_bytes(
            cert.public_bytes(serialization.Encoding.PEM)
        )

    @staticmethod
    def tls_get_fingerprint_from_cert(cert_file: Path, algorithm: str = "sha256"):
        """

        Args:
            cert_file:
            algorithm:

        Returns:

        """
        cert = x509.load_pem_x509_certificate(cert_file.read_bytes(), default_backend())
        results = cert.fingerprint(hashes.SHA256())
        return results.hex(":")
        
    @staticmethod
    def tls_create_pkcs23_pem_and_cert(private_key_file: Path, cert_file: Path,
                                       combined_file: Path):
        """

        Args:
            private_key_file:
            cert_file:
            combined_file:

        Returns:

        """
        with combined_file.open("wb") as fp:
            fp.write(private_key_file.read_bytes() + b"\n" + 
                     cert_file.read_bytes() + b"\n")