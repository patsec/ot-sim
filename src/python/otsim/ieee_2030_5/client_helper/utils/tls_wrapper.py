import subprocess
from pathlib import Path
from client_helper.utils import TLSWrap
import logging

_log = logging.getLogger(__name__)

class OpensslWrapper(TLSWrap):
    opensslcnf: Path = None

    def __init__(self, opensslconf: Path):
        OpensslWrapper.opensslcnf = opensslconf

    @staticmethod
    def __set_cnf_from_cert_path___(path: Path):
        if OpensslWrapper.opensslcnf is None:
            check_path = path.parent.parent.joinpath("openssl.cnf")
            if check_path.exists():
                OpensslWrapper.opensslcnf = check_path

    @staticmethod
    def tls_create_private_key(file_path: Path):
        OpensslWrapper.__set_cnf_from_cert_path___(file_path)
        # openssl ecparam -out private/ec-cakey.pem -name prime256v1 -genkey
        cmd = ["openssl", "ecparam", "-out", str(file_path), "-name", "prime256v1", "-genkey"]
        return subprocess.check_output(cmd, text=True)

    @staticmethod
    def tls_create_ca_certificate(common_name: str, private_key_file: Path, ca_cert_file: Path):
        OpensslWrapper.__set_cnf_from_cert_path___(ca_cert_file)
        # openssl req -new -x509 -days 3650 -config openssl.cnf \
        #   -extensions v3_ca -key private/ec-cakey.pem -out certs/ec-cacert.pem
        cmd = [
            "openssl", "req", "-new", "-x509", "-days", "3650", "-subj", f"/C=US/CN={common_name}",
            "-config",
            str(OpensslWrapper.opensslcnf), "-extensions", "v3_ca", "-key",
            str(private_key_file), "-out",
            str(ca_cert_file)
        ]
        _log.debug(" ".join(cmd))
        return subprocess.check_output(cmd, text=True)

    @staticmethod
    def tls_create_csr(common_name: str, private_key_file: Path, server_csr_file: Path):
        OpensslWrapper.__set_cnf_from_cert_path___(private_key_file)
        subject_name = common_name.split(":")[0]
        # openssl req -new -key server.key -out server.csr -sha256
        cmd = [
            "openssl", "req", "-new", "-config",
            str(OpensslWrapper.opensslcnf), "-subj", f"/C=US/CN={subject_name}", "-key",
            str(private_key_file), "-out",
            str(server_csr_file), "-sha256"
        ]
        return subprocess.check_output(cmd, text=True)

    @staticmethod
    def tls_create_signed_certificate(common_name: str,
                                      ca_key_file: Path,
                                      ca_cert_file: Path,
                                      private_key_file: Path,
                                      cert_file: Path,
                                      as_server: bool = False):
        OpensslWrapper.__set_cnf_from_cert_path___(cert_file)
        subject_name = common_name.split(":")[0]
        csr_file = Path(f"/tmp/{common_name}")
        OpensslWrapper.tls_create_csr(common_name, private_key_file, csr_file)
        # openssl ca -keyfile /root/tls/private/ec-cakey.pem -cert /root/tls/certs/ec-cacert.pem \
        #   -in server.csr -out server.crt -config /root/tls/openssl.cnf
        cmd = [
            "openssl",
            "ca",
            "-keyfile",
            str(ca_key_file),
            "-cert",
            str(ca_cert_file),
            "-subj",
            f"/C=US/CN={subject_name}",
            "-in",
            str(csr_file),
            "-out",
            str(cert_file),
            "-config",
            str(OpensslWrapper.opensslcnf),
        # For no prompt use -batch
            "-batch"
        ]
        # if as_server:
        #     "-server"
        print(" ".join(cmd))
        ret_value = subprocess.check_output(cmd, text=True)
        csr_file.unlink()
        return ret_value

    @staticmethod
    def tls_get_fingerprint_from_cert(cert_file: Path, algorithm: str = "sha256") -> str:
        OpensslWrapper.__set_cnf_from_cert_path___(cert_file)
        if algorithm == "sha256":
            algorithm = "-sha256"
        else:
            raise NotImplementedError()

        cmd = ["openssl", "x509", "-in", str(cert_file), "-noout", "-fingerprint", algorithm]
        ret_value = subprocess.check_output(cmd, text=True)
        if "=" in ret_value:
            ret_value = ret_value.split("=")[1].strip()
        return ret_value

    @staticmethod
    def tls_create_pkcs23_pem_and_cert(private_key_file: Path, cert_file: Path,
                                       combined_file: Path):
        OpensslWrapper.__set_cnf_from_cert_path___(cert_file)
        # openssl pkcs12 -export -in certificate.pem -inkey privatekey.pem -out cert-and-key.pfx
        tmpfile = Path("/tmp/tmp.p12")
        tmpfile2 = Path("/tmp/all.pem")
        tmpfile.unlink(missing_ok=True)
        cmd = [
            "openssl", "pkcs12", "-export", "-in",
            str(cert_file), "-inkey",
            str(private_key_file), "-out",
            str(tmpfile), "-passout", "pass:"
        ]
        subprocess.check_output(cmd, text=True)

        # openssl pkcs12 -in path.p12 -out newfile.pem -nodes
        cmd = [
            "openssl", "pkcs12", "-in",
            str(tmpfile), "-out",
            str(tmpfile2), "-nodes", "-passin", "pass:"
        ]
        # cmd = ["openssl", "pkcs12", "-in", str(tmpfile),
        # "-out", str(combined_file), "-clcerts", "-nokeys", "-passin", "pass:"]
        # -clcerts -nokeys
        subprocess.check_output(cmd, text=True)

        with open(combined_file, "w") as fp:
            in_between = False
            for line in tmpfile2.read_text().split("\n"):
                if not in_between:
                    if "BEGIN" in line:
                        fp.write(f"{line}\n")
                        in_between = True
                else:
                    fp.write(f"{line}\n")
                    if "END" in line:
                        in_between = False
